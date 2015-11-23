from netCDF4 import Dataset, numpy
import numpy as np
import os
from glob import glob
from contextlib import contextmanager


def flatten(lst):
    result = []
    for element in lst:
        if hasattr(element, "__iter__") and not isinstance(element, str):
            result.extend(flatten(element))
        else:
            result.append(element)
    return result


DTYPES = {}
DTYPES[numpy.dtype('float32')] = 'f4'
DTYPES[numpy.dtype('int32')] = 'i4'
DTYPES[numpy.dtype('int8')] = 'i1'
DTYPES[numpy.dtype('S1')] = 'S1'


class NCObject(object):

    @classmethod
    def open(cls, files_or_pattern):
        files, pattern = cls.distill(files_or_pattern)
        obj = cls.choice_type(files)
        obj.pattern = pattern
        obj.load()
        return obj

    @classmethod
    def choice_type(cls, files):
        return NCPackage(files) if len(files) > 1 else NCFile(files)

    @classmethod
    def distill(cls, files_or_pattern):
        if files_or_pattern in ['', []]:
            raise Exception('There is not file list or '
                            'pattern to open.')
        if files_or_pattern.__class__ is list:
            files = files_or_pattern
            pattern = ''
        else:
            files = glob(files_or_pattern)
            pattern = files_or_pattern
        if not len(files):
            files = [files_or_pattern]
            pattern = files_or_pattern
        return files, pattern

    def __init__(self, files):
        super(NCObject, self).__init__()
        self.files = files
        self.files.sort()
        self.variables = {}
        self._is_new = [not os.path.exists(f) for f in self.files]
        self.roots = []
        self.variable_wrapper = lambda name, vars: name, vars
        self.create_dim = 'create_dimension'
        self._read_only = True

    @property
    def is_new(self):
        return all(self._is_new)

    @property
    def dimensions(self):
        dicts = [r.dimensions for r in self.roots]
        keys = {k for d in dicts for k in d}
        return {k: flatten([d.get(k) for d in dicts])
                for k in keys}

    def has_dimension(self, name):
        return all([name in r.dimensions.keys() for r in self.roots])

    def create_dimension(self, name, size):
        return [getattr(r, self.create_dim)(name, size) for r in self.roots]

    def obtain_dimension(self, name):
        return self.dimensions[name]

    def getdim(self, name, size=None):
        return (self.obtain_dimension(name)
                if self.has_dimension(name)
                else self.create_dimension(name, size))

    def obtain_variable(self, *args, **kwargs):
        raise Exception('Subclass responsability (should process %s and %s)' %
                        (str(args), str(kwargs)))

    def getvar(self, name, vtype='', dimensions=(), digits=0,
               fill_value=None, source=None):
        if source:
            self.copy_in(name, vtype, source)
        if name not in self.variables.keys():
            varstmp = self.obtain_variable(name, vtype, dimensions,
                                           digits, fill_value)
            self.variables[name] = self.variable_wrapper(name, varstmp)
        return self.variables[name]

    def sync(self):
        return [r.sync() for r in self.roots]

    def close(self):
        return [r.close() for r in self.roots]

    def copy_in(self, name, vtype, source):
        # create dimensions if not exists.
        dims = source.dimensions
        gt1_or_none = lambda x: len(x) if len(x) > 1 else None
        create_dim = lambda d: self.getdim(d, gt1_or_none(dims[d]))
        list(map(create_dim, dims))
        dimensions = tuple(reversed([str(k)
                                     for k in source.dimensions.keys()]))
        vtype_tmp = vtype if vtype else source.vtype
        options = {'fill_value': 0.0}
        if vtype_tmp == 'f4':
            options['digits'] = source.least_significant_digit
        var = self.getvar(name, vtype_tmp, dimensions, **options)
        var[:] = source[:]


class NCFile(NCObject):

    def load(self):
        filename = self.files[0]
        try:
            self.roots = [(Dataset(filename, mode='w', format='NETCDF4')
                           if self.is_new else Dataset(filename, mode='a',
                                                       format='NETCDF4'))]
            self._read_only = False
        except Exception:
            self.roots = [Dataset(filename, mode='r', format='NETCDF4')]
        self.variable_wrapper = SingleNCVariable
        self.create_dim = 'createDimension'

    @property
    def read_only(self):
        return self._read_only

    def obtain_variable(self, name, vtype='f4', dimensions=(), digits=0,
                        fill_value=None):
        root = self.roots[0]
        return (root.variables[name] if name in root.variables.keys()
                else self.create_variable(name, vtype, dimensions,
                                          digits, fill_value))

    def create_variable(self, name, vtype='f4', dimensions=(), digits=0,
                        fill_value=None):
        build = self.roots[0].createVariable
        options = {'zlib': True,
                   'fill_value': fill_value}
        if digits > 0:
            options['least_significant_digit'] = digits
        varstmp = [build(name, vtype, dimensions, **options)]
        not_auto_mask = lambda v: v.set_auto_maskandscale(False)
        list(map(not_auto_mask, varstmp))
        return varstmp


class NCPackage(NCObject):

    def load(self):
        self.roots = [NCObject.open(filename) for filename in self.files]
        self.variable_wrapper = DistributedNCVariable

    @property
    def read_only(self):
        return all([r.read_only for r in self.roots])

    def obtain_variable(self, name, vtype='f4', dimensions=(), digits=0,
                        fill_value=None):
        return [r.getvar(name, vtype, dimensions, digits, fill_value)
                for r in self.roots]


class NCVariable(object):

    def __init__(self, name, variables):
        self.name = name
        self.variables = (variables
                          if variables.__class__ is list else [variables])

    def __eq__(self, obj):
        return (self.pack() == obj[:]).all()

    @property
    def shape(self):
        return self.pack().shape

    @property
    def dimensions(self):
        var = self.variables[0]
        dims = dict(var.group().dimensions)
        return {d: dims[d] for d in var.dimensions}

    @property
    def least_significant_digit(self):
        variable = self.variables[0]
        return (variable.least_significant_digit
                if hasattr(variable, 'least_significant_digit') else 0)

    @property
    def dtype(self):
        return self.variables[0].dtype

    @property
    def vtype(self):
        return DTYPES[np.dtype(self.dtype)]

    def __getitem__(self, indexes):
        return self.pack().__getitem__(indexes)

    def __getattr__(self, name):
        print 'Unhandled [class: %s, instance: %s, attr: %s]' % (
            self.__class__, self.name, name)
        import ipdb
        ipdb.set_trace()

    def sync(self):
        for variable in self.variables:
            variable.group().sync()


class SingleNCVariable(NCVariable):

    def group(self):
        return self.variables[0].group()

    def pack(self):
        varstmp = self.variables[0]
        if self.variables[0].shape[0] > 1:
            varstmp = np.vstack([self.variables])
        return varstmp

    def __setitem__(self, indexes, changes):
        return self.variables[0].__setitem__(indexes, changes)


class DistributedNCVariable(NCVariable):

    def pack(self):
        return np.vstack([variable.pack() for variable in self.variables])

    def __setitem__(self, indexes, change):
        pack = self.pack()
        pack.__setitem__(indexes, change)
        varstmp = np.vsplit(pack, pack.shape[0])
        for i in range(len(varstmp)):
            self.variables[i][:] = varstmp[i]
        self.sync()


def open(pattern):
    """
    Return a root descriptor to work with one or multiple NetCDF files.

    Keyword arguments:
    pattern -- a list of filenames or a string pattern.
    """
    root = NCObject.open(pattern)
    return root, root.is_new


def getdim(root, name, size=None):
    """
    Return a dimension from a NCFile or NCPackage instance. If the dimension
    doesn't exists create it.

    Keyword arguments:
    root -- the root descriptor returned by the 'open' function
    name -- the name of the dimension
    size -- the size of the dimension, if it has a fixed size (default None)
    """
    return root.getdim(name, size)


def getvar(root, name, vtype='', dimensions=(), digits=0, fill_value=None,
           source=None):
    """
    Return a variable from a NCFile or NCPackage instance. If the variable
    doesn't exists create it.

    Keyword arguments:
    root -- the root descriptor returned by the 'open' function
    name -- the name of the variable
    vtype -- the type of each value, ex ['f4', 'i4', 'i1', 'S1'] (default '')
    dimensions -- the tuple with dimensions name of the variables (default ())
    digits -- the precision required when using a 'f4' vtype (default 0)
    fill_value -- the initial value used in the creation time (default None)
    source -- the source variable to be copied (default None)
    """
    return root.getvar(name, vtype, dimensions, digits, fill_value, source)


def sync(root):
    """
    Force the root descriptor to synchronize writing the buffers to the disk.

    Keyword arguments:
    root -- the root descriptor returned by the 'open' function
    """
    root.sync()


def close(root):
    """
    Close the root descriptor and write the buffer to the disk.

    Keyword arguments:
    root -- the root descriptor returned by the 'open' function
    """
    root.close()


@contextmanager
def loader(pattern):
    root, _ = open(pattern)
    yield root
    close(root)
