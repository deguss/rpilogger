#!/usr/bin/python -u

_LOCATION_IN = "../data/tmp"
import numpy as np
import matplotlib.pyplot as plt

ch1=np.zeros(1000*60,dtype='float32')
ch2=np.zeros(1000*60,dtype='float32')
t=np.zeros(1000*60,dtype='float32')

def readfiles():
    import os, re, time
    from datetime import datetime as datetime
    import netcdf    
    global t, ch1, ch2
    
    files = sorted(os.listdir(_LOCATION_IN))
    d=1
    while not re.search('[1-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9].nc',files[-d]):
        d+=1
    fn=int(files[-d][:-3]) #remove ending ".nc" and convert to seconds
    ts1=time.gmtime(fn)    #broken down time format
    print "%d files found, most recent: %d.nc (%s)" % (len(files), fn, time.strftime("%F %T %z",ts1))
    
    try:
        fid = netcdf.Dataset(os.path.join(_LOCATION_IN,files[-d]), 'r')
    except:
        print "Unexpected error while reading file: %s \n %s" % (files[-d])
        return -1
    else:
        start = getattr(fid,'start')      #some string
        sps = getattr(fid,'sps')          #sampling rate float
        #import pdb; pdb.set_trace()                
        sampl = fid.variables['ch1'].size 
        units = fid.variables['ch1'].units
        print "\tstart time         = %s" % (start)
        print "\tsamples            = %d" % (sampl)
        print "\tsampling frequency = %f" % (sps)
        
        ch1=fid.variables['ch1'][:]
        ch2=fid.variables['ch2'][:]
        t=np.linspace(0,60,sampl)
        fid.close()
        
def plot_time():
    global t, ch1, ch2
    
    plt.plot(t, ch1)
    plt.xlabel('time (s)')
    plt.ylabel('voltage (mV)')
    plt.title('ch1')
    plt.grid(True)
    plt.savefig("ch1.png")
    plt.show()

def plot_psd():
    dt = 0.01
    t = np.arange(0, 10, dt)
    nse = np.random.randn(len(t))
    r = np.exp(-t/0.05)

    cnse = np.convolve(nse, r)*dt
    cnse = cnse[:len(t)]
    s = 0.1*np.sin(2*np.pi*t) + cnse

    plt.subplot(211)
    plt.plot(t, s)
    plt.subplot(212)
    plt.psd(s, 512, 1/dt)

    plt.show()

if __name__ == "__main__":
    readfiles()
    plot_time()
    plot_psd()
