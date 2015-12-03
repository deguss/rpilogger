#!/usr/bin/python -u

_LOCATION_IN = "/home/pi/data/"
_LOCATION_CHARTS = "/home/pi/www/charts"
import os, sys

if len(sys.argv) != 2:
    print 'usage: ./readin.py \"file.nc\"'
    sys.exit(-1)
    
fname=sys.argv[1]
filename=os.path.join(_LOCATION_IN,fname)
print "reading file %s..." % (filename)
    
import numpy as np
from datetime import datetime as datetime
import netcdf    
    
try:
    fid = netcdf.Dataset(filename, 'r')
except:
    print "Unexpected error while reading file: %s" % (filename)
    sys.exit(-1)
else:
    start = getattr(fid,'start')      #string, however  \0 missing
    #import pdb; pdb.set_trace()
    start=start[:26]
    sps = int(getattr(fid,'sps'))     #sampling rate -> to int
    sampl = fid.variables['ch1'].size 
    units = fid.variables['ch1'].units
    print "\tstart time         = %s" % (start)
    print "\tsamples            = %d" % (sampl)
    print "\tsampling frequency = %d" % (sps)
    

    ch1=np.zeros(sampl,dtype='float32')
    ch2=np.zeros(sampl,dtype='float32')
    t=np.zeros(sampl,dtype='float32')
    
    ch1=fid.variables['ch1'][:]
    ch2=fid.variables['ch2'][:]
    t=np.linspace(0,60,sampl)
    fid.close()
print "file read"

cnan1 = np.count_nonzero(np.isnan(ch1))
cnan2 = np.count_nonzero(np.isnan(ch2))
print "ch1: %i NaN, ch2: %i NaN" % (cnan1,cnan2)
print
m=sps*60
print " ----- valid samples ----- "
print "min\tch1\tch2"
for c in range(0,60):
    cnan1 = np.count_nonzero(~np.isnan(ch1[c*m:(c+1)*m]))
    cnan2 = np.count_nonzero(~np.isnan(ch2[c*m:(c+1)*m]))
    print "%i.\t%i\t%i" %(c+1,cnan1,cnan2)
else:
    print 
