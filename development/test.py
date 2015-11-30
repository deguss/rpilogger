#!/usr/bin/python -u
import ConfigParser

config = ConfigParser.ConfigParser()
config.readfp(open(r'../catnc.conf'))
print config.get('default', '_LOCATION_IN')
print config.get('default', '_LOCATION_OUT')
import pdb; pdb.set_trace()
print config.get('default', '_REMOTES')


