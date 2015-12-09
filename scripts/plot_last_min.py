#!/usr/bin/python -u
from tendo import singleton
me = singleton.SingleInstance() # will sys.exit(-1) if other instance is running

_LOCATION_IN = "/home/pi/data/tmp"
_LOCATION_CHARTS = "/home/pi/www/charts"
import numpy as np
import matplotlib
matplotlib.use('Agg') #no window should apper when drawing.
import matplotlib.pyplot as plt
import matplotlib.mlab as mlab
import os, sys

ch1=np.zeros(1000*60,dtype='float32')
ch2=np.zeros(1000*60,dtype='float32')
t=np.zeros(1000*60,dtype='float32')

def readfiles():
    import re, time
    from datetime import datetime as datetime
    import netcdf
    import socket
    
    global t, ch1, ch2, sps, sampl, title
    
    files = sorted(os.listdir(_LOCATION_IN))
    d=len(files)
    fn=int(files[-1][:-3]) #remove ending ".nc" and convert to seconds
    ts1=time.gmtime(fn)    #broken down time format
    title=time.strftime("%F %T %z",ts1)+' on '+socket.gethostname()
    print "%d files found, most recent: %d.nc (%s)" % (len(files), fn, title)
    
    try:
        fid = netcdf.Dataset(os.path.join(_LOCATION_IN,files[-1]), 'r')
    except:
        print "Unexpected error while reading file: %s" % (os.path.join(_LOCATION_IN,files[-1]))
        return -1
    else:
        start = getattr(fid,'start')      #some string
        sps = int(getattr(fid,'sps'))     #sampling rate -> to int
        sampl = fid.variables['ch1'].size 
        units = fid.variables['ch1'].units
        print "\tstart time         = %s" % (start)
        print "\tsamples            = %d" % (sampl)
        print "\tsampling frequency = %d" % (sps)
        
        ch1=fid.variables['ch1'][:]
        ch2=fid.variables['ch2'][:]
        t=np.linspace(0,60,sampl)
        fid.close()
    print "file read"
    return 1
        
def plot_time():
    global t, ch1, ch2, sps, sampl, title
    
    plt.clf()
    
    ax1=plt.subplot(211)
    plt.plot(t, ch1, color='red')
    plt.ylabel('voltage (mV)')
    plt.grid(True)
    plt.title('time plot from '+title)
    
    
    
    ax2=plt.subplot(212)
    plt.plot(t, ch2, color='green')
    plt.xlabel('time (s)')
    plt.grid(True)
    
    ym=np.zeros(2)
    ym[0]=min(ax1.get_ylim()[0], ax2.get_ylim()[0])
    ym[1]=max(ax1.get_ylim()[1], ax2.get_ylim()[1])
    
    ax1.set_ylim(ym)
    ax2.set_ylim(ym)
    
    fig=plt.gcf();
    f_dpi=fig.get_dpi()
    fig.set_size_inches(1200.0/f_dpi, 500.0/f_dpi);
    
    plt.savefig(os.path.join(_LOCATION_CHARTS,"last_time.png"), dpi=96, bbox_inches='tight')
    plt.clf()
    print "plotting time series finished"

def plot_psd():
    global t, ch1, ch2, sps, sampl, title
    
    plt.clf()
    nfft=2048 #int(2**np.ceil(np.log2(sampl)));

    ax1=plt.subplot(211)
    (Pxx, freqs)=plt.psd(ch1, NFFT=nfft, Fs=sps, window=mlab.window_hanning)
    plt.title('PSD (NFFT='+str(nfft)+', Fs='+str(sps)+'Hz, hann) from '+title)
    plt.xlabel('')

    ax2=plt.subplot(212)
    (Pxx, freqs)=plt.psd(ch2, NFFT=nfft, Fs=sps, window=mlab.window_hanning)
    plt.ylabel('')
    ax2.set_ylim(ax1.get_ylim())

    fig=plt.gcf()
    f_dpi=fig.get_dpi()    
    fig.set_size_inches(1200.0/f_dpi, 500.0/f_dpi);

    plt.savefig(os.path.join(_LOCATION_CHARTS,"last_psd.png"), dpi=96, bbox_inches='tight')
    plt.clf()
    print "plotting psd finished"

def plot_psd_resampled():
    global t, ch1, ch2, sps, sampl, title
    
    plt.clf()
    nfft=2048 #int(2**np.ceil(np.log2(sampl)));

    ax1=plt.subplot(211)
    (Pxx, freqs)=plt.psd(ch1[::5], NFFT=nfft, Fs=sps/5, window=mlab.window_hanning, color='red')
    plt.title('PSD (NFFT='+str(nfft)+', Fs='+str(sps/5)+'Hz, hann) from '+title)
    plt.xlabel('')

    ax2=plt.subplot(212)
    (Pxx, freqs)=plt.psd(ch2[::5], NFFT=nfft, Fs=sps/5, window=mlab.window_hanning, color='green')
    plt.ylabel('')
    ax2.set_ylim(ax1.get_ylim())

    fig=plt.gcf();
    f_dpi=fig.get_dpi()
    #print "dpi: %i" % (f_dpi)
    
    #f_size=fig.get_size_inches()
    #print "figure size in Inches", f_size
    #print "or pixels %i x %i pixel" % (f_dpi*f_size[0], f_dpi*f_size[1])
    #fig.set_size_inches([f_size[0]*2, f_size[1]]);
    
    fig.set_size_inches(1200.0/f_dpi, 500.0/f_dpi);
    
    #f_size=fig.get_size_inches()
    #print "resized to %i x %i pixel" % (f_dpi*f_size[0], f_dpi*f_size[1])

    
    plt.savefig(os.path.join(_LOCATION_CHARTS,"last_psd_res.png"), dpi=96, bbox_inches='tight')
    plt.clf()
    print "plotting psd_resampled finished"
    

if __name__ == "__main__":
    if (readfiles() < 0):
        exit(1)
    plot_time()
    plot_psd()
    plot_psd_resampled()
    sys.stdout.flush()
