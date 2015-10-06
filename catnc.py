#!/usr/bin/python -u
import datetime, os, sys, stat, subprocess, re, shutil, time
import select
import numpy
import netcdf
# ------- SETUP --------------------
_LOCATION_IN = "/home/pi/data/" # define global variable as "protected" - begins with _
_LOCATION_OUT = "/home/pi/dp/"
_REMOTE = "pid@aero.nck.ggki.hu"
_DEFAULT_LEN = 24*60
_FILE_SECS = 60.0
# ---------------------------------
d_c = numpy.zeros(30000*_DEFAULT_LEN+30000,dtype='float32') #sure < sys.maxint
d_d = numpy.zeros(30000*_DEFAULT_LEN+30000,dtype='float32')
processed=[]
differences=[]
i_mode=""

def checkUser():
    i,o,e = select.select([sys.stdin],[],[],0.0001)
    for s in i:
        if s == sys.stdin:
            input = sys.stdin.readline()
            return True
    return False

def filesize(loc):
    suffixes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB']
    nbytes = os.path.getsize(loc)
    if nbytes == 0: return '0 B'
    i = 0
    while nbytes >= 1024 and i < len(suffixes)-1:
        nbytes /= 1024.
        i += 1
    f = ('%.2f' % nbytes).rstrip('0').rstrip('.')
    return '%s %s' % (f, suffixes[i])

def mkdir(directory):
    if not os.path.exists(directory):
        try:
            os.makedirs(directory)
        except OSError as e:
            print "Error while creating directory %s!\n %s" % (directory,e)
            exit(-1)


def concatenate(daydir,resdir):
    global d_c, d_d, processed, differences, i_mode

    
    files = [w for w in sorted(os.listdir(daydir))]
    print "%d files found, will process %s" % (len(files), "all" if (lim==_DEFAULT_LEN) else str(lim))
    if (len(files) < _DEFAULT_LEN):
        print "files are missing. There should be 24*60=1440"
    if (i_mode): #interactive mode
        print "\t(press return (enter) to gently abort)"

    processed=[]
    differences=[]
    nans=0
    d_ix=0
    for f in (f for f in files if (re.search('[0-2][0-9][0-5][0-9][0-5][0-9].[0-9][0-9][0-9][0-9].nc',f))): #(f.endswith(".nc"))):
        processed.append(datetime.datetime.strptime(dd+f,"%Y/%m/%d/%H%M%S.%f.nc"))
        d_lim = processed[0]+datetime.timedelta(hours=1)
        d_lim = d_lim.replace(minute=0,second=0,microsecond=0)
        try:
            fid = netcdf.Dataset(os.path.join(daydir,f), 'r')
        except IOError as e:
            print "Unexpected error while reading file: %s \n %s" % (f, e)
            return -1
        except RuntimeError as e: 
            # if file is damadged simply skip it
            print "Unexpected error while reading file: %s \n %s" % (f, e)
            continue            
        else:
            if (len(processed)==1):
                start = getattr(fid,'start')
                sps = getattr(fid,'sps') #500
                sampl = fid.variables['ch3'].size #30000
                units = fid.variables['ch3'].units #mV
                print "first file: %s has %d samples (%d seconds at %d Hz)" % (f, sampl, sampl/sps, sps)
                if ((processed[0]-dy).total_seconds() < _FILE_SECS): # if right after midnight 
                    dz = dy-datetime.timedelta(days=1)  #the day before
                    dzt = dz.strftime('%Y/%m/%d/')
                    dzdir = os.path.join(_LOCATION_IN,dzt)
                    if not os.path.exists(dzdir): #try to get last file of previous day
                        print "could not find the day before (%s) to obtain the file spreading midnight" % dzdir    
                    else:
                        filep = sorted(os.listdir(dzdir))[-1]
                        fp_st = datetime.datetime.strptime(dzt+filep,"%Y/%m/%d/%H%M%S.%f.nc")
                        oldd_s = datetime.timedelta.total_seconds(dy-fp_st)
                        if (oldd_s < _FILE_SECS): #if file covers midnight
                            fp_y=os.path.join(_LOCATION_IN,dzt+filep)
                            od_s = _FILE_SECS - oldd_s
                            print "file of day before (%s) contains %.4f seconds of current day" % (dzt+filep,od_s)
                            try:
                                fp = netcdf.Dataset(fp_y, 'r')
                            except (IOError, RuntimeError) as e:
                                print "Unexpected error while reading file: %s \n %s" % (fp_y, e)
                                continue
                            else:
                                if (getattr(fp,'sps') == sps and fp.variables['ch3'].size == sampl):
                                    si = int(numpy.round(oldd_s * sps))
                                    inn=fp.variables['ch3'][si:].size
                                    d_c[d_ix:d_ix+inn] = fp.variables['ch3'][si:]
                                    d_d[d_ix:d_ix+inn] = fp.variables['ch4'][si:]
                                    d_ix+=inn
                                    print "imported %d values from %s" %(inn,dzt+filep)
                                    processed[0] = dy
                                    start = dy.strftime("%Y-%m-%d %H:%M:%S.%f")
                                else:
                                    print "Error while processing %s. File inconsistent!" %(fp_y)
                                    return -1
                                fp.close()
                #import pdb; pdb.set_trace()
                d_c[d_ix:d_ix+fid.variables['ch3'].size] = fid.variables['ch3'][:]  # like append
                d_d[d_ix:d_ix+fid.variables['ch3'].size] = fid.variables['ch4'][:]  #           
                d_ix+=fid.variables['ch3'].size
            if (len(processed)>1):
                try:
                    fid2 = netcdf.Dataset(os.path.join(daydir,f), 'r')
                except (IOError,RuntimeError) as e:
                    print "Unexpected error while reading file: %s \n %s" % (f, e)
                    continue
                else:
                    if (sps != getattr(fid2,'sps')):
                        print "sampling rate inconsistent at %s! %d, now: %d" % (f, sps, getattr(fid2,'sps'))
                        break
                    if (sampl != fid2.variables['ch3'].size):
                        print "file length differs at %s! %d, now: %d" % (f, sampl, fid2.variables['ch3'].size)
                        break

                    if (len(processed)>2): #ignore first interval due to its possibly bigger range
                        time_delta = processed[-1] - processed[-2]
                        differences = numpy.hstack((differences, time_delta.total_seconds()))
                        m=int(numpy.round((differences[-1]-_FILE_SECS)*sps))
                        if (m != 0):
                            if (m > 0):
                                print "WARN: %3d samples are missing. inserted %d*NaN" % (m,m)
                                print "\t %s" % (processed[-2].strftime("%H:%M:%S.%f"))
                                print "\t %s" % (processed[-1].strftime("%H:%M:%S.%f"))
                                print "   diff: %10.4f seconds" % (differences[-1]-_FILE_SECS)
                                if (processed[-1] >= d_lim): # if more files are missing so that next hour is also rare
                                    time_delta = d_lim - processed[-2]
                                    m=int(numpy.round((time_delta.total_seconds()-_FILE_SECS)*sps))
                                    print "WARN: even more files are missing\n will insert only %d*NaN (%10.4f secs) and skip some files" % (m,(float)(m)/sps)
                                    d_ins = numpy.empty((m))
                                    d_ins[:] = numpy.NAN
                                    d_c[d_ix:d_ix+m] = d_ins
                                    d_d[d_ix:d_ix+m] = d_ins
                                    d_ix+=m
                                    nans+=m
                                    stop = d_lim.strftime("%Y-%m-%d %H:%M:%S.%f")
                                    differences[-1]=time_delta.total_seconds()
                                    if (savebin(resdir=resdir, d_ix=d_ix, start=start, stop=stop, nans=nans, sps=sps, units=units)):
                                        return -1
                                    processed=[processed[-1]]
                                    differences=[]
                                    nans=0
                                    d_ix=0
                                    d_lim = processed[0]+datetime.timedelta(hours=1)
                                    d_lim = d_lim.replace(minute=0,second=0,microsecond=0)
                                    inn=fid2.variables['ch3'][:].size
                                    d_c[d_ix:d_ix+inn] = fid2.variables['ch3'][:]
                                    d_d[d_ix:d_ix+inn] = fid2.variables['ch4'][:]
                                    d_ix+=inn
                                    start=stop
                                    #import pdb; pdb.set_trace()
                                else:
                                    d_ins = numpy.empty((m))
                                    d_ins[:] = numpy.NAN
                                    d_c[d_ix:d_ix+m] = d_ins
                                    d_d[d_ix:d_ix+m] = d_ins
                                    d_ix+=m
                                    nans+=m
                            elif (m < 0):
                                print "WARN: files to dense! %d samples seems to be to much before %s" % (m, f)
                                print "\t %s" % (processed[-2].strftime("%H:%M:%S.%f"))
                                print "\t %s" % (processed[-1].strftime("%H:%M:%S.%f"))
                                print "   diff: %10.4f seconds" % (differences[-1]-_FILE_SECS)
                    #check if file not too long
                    if (processed[-1] < d_lim): # present file yet in batch
                        rs = (d_lim - processed[-1]).total_seconds()
                        if (rs < _FILE_SECS): #however if it is the last one, which needs to be truncated
                            si = int(numpy.round(rs * sps))
                            d_c[d_ix:d_ix+si] = fid2.variables['ch3'][:si]
                            d_d[d_ix:d_ix+si] = fid2.variables['ch4'][:si]
                            d_ix+=si
                            print "from last file %s only %f seconds (%d samples) taken" %(f, rs, si)
                            stop = d_lim.strftime("%Y-%m-%d %H:%M:%S.%f")
                            if (savebin(resdir=resdir, d_ix=d_ix, start=start, stop=stop, nans=nans, sps=sps, units=units)):
                                return -1
                            #import pdb; pdb.set_trace()
                            processed=[]
                            differences=[]
                            nans=0
                            d_ix=0
                            inn=fid2.variables['ch3'][si:].size
                            d_c[d_ix:d_ix+inn] = fid2.variables['ch3'][si:]
                            d_d[d_ix:d_ix+inn] = fid2.variables['ch4'][si:]
                            d_ix+=inn
                            start=stop
                            processed.append(d_lim)
                            
                        else:
                            d_c[d_ix:d_ix+fid2.variables['ch3'].size] = fid2.variables['ch3'][:] # append
                            d_d[d_ix:d_ix+fid2.variables['ch3'].size] = fid2.variables['ch4'][:]
                            d_ix+=fid2.variables['ch3'].size
                    fid2.close()
            fid.close()
        if (i_mode and checkUser()):
            print "user interrupt! %d files processed\n" %(len(processed))
            return -1
            break
    return len(files)


def savebin(resdir, d_ix, start, stop, nans, sps, units):
    global d_c, d_d, processed, differences
    try:
        p
    except NameError:
        p=""
    else:
        print "waiting for scp"
        sts = os.waitpid(p.pid, 0)

    print " %d files processed with in total %d records" %(len(processed), d_ix)
    print " %d NaN inserted in total (%.3f seconds)" %(nans, nans/sps)
    if (len(processed) >= 2):
        s_intervals = "%.4f/%.4f/%.7f/%.7f" % (max(differences), min(differences), numpy.mean(differences),numpy.std(differences))
        print " intervals maximum/minimum/mean/std: (%s) s" % s_intervals
        jitter = (differences -60 )*1000
        s_jitter = "%+.4f/%.4f/%.7f/%.7f" % (max(jitter), min(jitter), numpy.mean(jitter),numpy.std(jitter))
        print " jitter    maximum/minimum/mean/std: (%s) ms" % s_jitter
        #print "(jitter = nominal - actual = 60.0000 - x)"

        try: #create file
            resfile = processed[0].strftime("%Y-%m-%dT%H.nc")
            resf = os.path.join(resdir,resfile)
            remf = processed[0].strftime("%Y/%m/%d/")
            if (os.path.exists(resf) and os.path.getsize(resf)>1*1024L*1024L):
                print " files already concatenated! Overwriting %s (%s)" %(resf,filesize(resf))
            print " writing data to file %s" % resf
            if (i_mode): #interactive mode
                print " please have patience, this might need several minutes"
            fidw = netcdf.Dataset(os.path.join(resdir,resfile), 'w', format='NETCDF4')
            fidw.setncatts({'files':len(processed), 'sps':sps, 'nan':nans,\
                            'start':start, 'stop':stop, 'timezone':'UTC',\
                            'intervals': s_intervals, 'jitter': s_jitter})
            fidw.createDimension('NS',d_ix)
            fidw.createDimension('WE',d_ix)
            fNS=fidw.createVariable('NS',numpy.float32,('NS',),zlib=True) #TODO  fid.variables['ch3'].dtype
            fWE=fidw.createVariable('WE',numpy.float32,('WE',),zlib=True) #TODO sign reversed
            fNS.units=units
            fWE.units=units
            # write data back
            fNS[:]=d_c[:d_ix]
            fWE[:]=d_d[:d_ix]
            print " writing %d records to file ..." % d_ix
            fidw.close()
        except:
            print " Unexpected error while writing to file: %s" % (resfile)
            print sys.exc_info()
            return -1
        else:
            print " %s written! Size: %s" % (resf, filesize(resf))
            pss = subprocess.Popen(["ssh "+_REMOTE+" 'mkdir -p lemi-data/"+remf+"'"], stdout=subprocess.PIPE, stderr=subprocess.PIPE,shell=True)
            output, errors = pss.communicate()
            if (errors):
                print "Could not create remote directory lemi-data/"+remf+" on server: "+_REMOTE+" !"
                print "Check for ssh keys and permissions!"
                print "No file copy (scp) possible"
            else :
                p = subprocess.Popen(["scp", resf, _REMOTE+":lemi-data/"+remf+resfile])
            #import pdb; pdb.set_trace()
            print " "
            return 0


if __name__ == "__main__":
    # ./catnc.py              --> non-interactive mode (processing yesterday)
    # ./catnc.py 2015-03-17   --> interactive: processing the given date (also format 2015/03/17 works)
    # ./catnc.py 2015-03-17 34
    i_mode=""
    lim=_DEFAULT_LEN
    if (len(sys.argv) > 1):
        try: # if first argument is limit
            lim=int(sys.argv[1])
        except ValueError: # if not, first is probably a date
            try: 
                i_mode=datetime.datetime.strptime(sys.argv[1],"%Y/%m/%d")
            except ValueError:
                try:
                    i_mode=datetime.datetime.strptime(sys.argv[1],"%Y-%m-%d")
                except ValueError: # else ignore first
                    i_mode=""
            try: # second argument a limit?
                lim=int(sys.argv[2])
            except (ValueError, IndexError):
                lim=_DEFAULT_LEN

    #redirect stdout and stderr
    _catnc_logfile = os.path.join(_LOCATION_OUT,"catnc.log")
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0) # Unbuffer output
    tee = subprocess.Popen(["tee", _catnc_logfile], stdin=subprocess.PIPE)
    os.dup2(tee.stdin.fileno(), sys.stdout.fileno())
    os.dup2(tee.stdin.fileno(), sys.stderr.fileno())
    
    print "starting %s " % ( ("for %s " %sys.argv[1]) if i_mode else "in non-interactive-mode")
    if (lim!=_DEFAULT_LEN):
        print "debug mode active with limit=%d" % lim
    t1 = datetime.datetime.now()
    dy = i_mode
    if not dy:
        d = datetime.datetime.now()
        print "Now it is " + str(d)
        dy = d-datetime.timedelta(days=1)
    dy = dy.replace(hour=0,minute=0,second=0,microsecond=0)
    dd = dy.strftime('%Y/%m/%d/')
    print "processing " + dd
    daydir = os.path.join(_LOCATION_IN,dd)
    resdir = os.path.join(_LOCATION_OUT,dy.strftime('%Y/%m/%d'))

    if not os.path.exists(daydir): 
        print "input directory %s does not exists! Will exit!" %(daydir)
        sys.exit(-1)

    mkdir(resdir)
    if os.listdir(resdir): 
        print "files possibly already concatenated. Directory %s exists." %(resdir)
        if (i_mode): #interactive mode
            if (raw_input("do you want to continue (y/n)?  ") == 'n'):
                sys.exit(-1)


    rv = concatenate(daydir=daydir,resdir=resdir)

    if (rv >= 0):
        print "concat ready, %d files processed of %s" % (rv,daydir)
        dby = dy-datetime.timedelta(days=4)
        dbdir = os.path.join(_LOCATION_IN,dby.strftime('%Y/%m/%d'))
        strq = "do you want to remove the files in %s ? (y/n) " % (dbdir)
        if (os.path.exists(dbdir) and ( (not i_mode) or (i_mode and raw_input(strq) == 'y'))): 
            pr = subprocess.Popen(["rm -r "+dbdir],stdout=subprocess.PIPE, stderr=subprocess.PIPE,shell=True)
            output, errors = pr.communicate()
            if (errors or output):
                print "output: %s, errors: %s" %(output,errors)
            else:
                print "files in %s deleted!" %(dbdir)

        dqy = dy-datetime.timedelta(days=10)
        dqdir = os.path.join(_LOCATION_OUT,dqy.strftime('%Y/%m/%d'))
        strq = "do you want to remove the files in %s ? (y/n) " % (dqdir)
        if (os.path.exists(dqdir) and ( (not i_mode) or (i_mode and raw_input(strq) == 'y'))): 
            remf=dqy.strftime('%Y/%m/%d')
            time.sleep(60) #wait upload to finish
            pss = subprocess.Popen(["ssh "+_REMOTE+" 'du -s lemi-data/"+remf+"'"], stdout=subprocess.PIPE, stderr=subprocess.PIPE,shell=True)
            output, errors = pss.communicate()
            size_remote=int(output.split(" ")[0])

            ss=subprocess.check_output(['du', '-s', dqdir]) 
            size_local=int(ss.split(" ")[0])

            if (size_local == size_remote):
                pr = subprocess.Popen(["rm -r "+dqdir],stdout=subprocess.PIPE, stderr=subprocess.PIPE,shell=True)
                output, errors = pr.communicate()
                if (errors or output):
                    print "output: %s, errors: %s" %(output,errors)
                else:
                    print "files in %s deleted!" %(dqdir)
            else:
                print "Error! Will not delete local directory! Size remote: %d, size local: %d" %(size_remote,size_local)

    else:
        print "Error! concatenate function returned %d\n" %(rv)


    td = datetime.datetime.now() - t1
    print "script run for %s (hours:mins:secs)" % str(td)
    print "\n"
    #import pdb; pdb.set_trace()
    shutil.copyfile(_catnc_logfile, os.path.join(resdir,"catnc.txt"))
    p = subprocess.Popen(["scp", os.path.join(resdir,"catnc.txt"), _REMOTE+":lemi-data/"+dy.strftime('%Y/%m/%d/')+"catnc.txt"])
    sts = os.waitpid(p.pid, 0)
    sys.exit(rv)

