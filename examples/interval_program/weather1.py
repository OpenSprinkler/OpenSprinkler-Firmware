#!/usr/bin/python
import urllib, urllib2, cgi
import json, datetime, time, sys
import pytz
from datetime import datetime, timedelta
def safe_float(s, dv):
  r = dv
  try:
    r = float(s)
  except:
    return dv
  return r

def safe_int(s, dv):
  r = dv
  try:
    r = int(s)
  except:
    return dv
  return r 

def isInt(s):
  try:
    _v = int(s)
  except:
    return 0
  return 1
  
def isFloat(s):
  try:
    _f = float(s)
  except:
    return 0
  return 1
  
def main():
  print 'Content-Type: text/html'
  print

  form = cgi.FieldStorage()
  loc = form.getfirst('loc', '')
  key = form.getfirst('key', '')
  of = form.getfirst('format','')

  # if loc is GPS coordinate itself
  sp = loc.split(',', 1)
  if len(sp)==2 and isFloat(sp[0]) and isFloat(sp[1]):
    lat = sp[0]
    lon = sp[1]
  else:
    lat = None
    lon = None
    
  # if loc is US 5+4 zip code, strip the last 4
  sp = loc.split('-', 1)
  if len(sp)==2 and isInt(sp[0]) and len(sp[0])==5 and isInt(sp[1]) and len(sp[1])==4:
    loc=sp[0]

  tzone = None  
  # if loc is pws, query wunderground geolookup to get GPS coordinates
  if loc.startswith('pws:'):
    try:
      req = urllib2.urlopen('http://api.wunderground.com/api/'+key+'/geolookup/q/'+loc+'.json')
      dat = json.load(req)
      if dat['location']:
        v = dat['location']['lat']
        if v and isFloat(v):
          lat = v
        v = dat['location']['lon']
        if v and isFloat(v):
          lon = v
        v = dat['location']['tz_long']
        if v:
          tzone = v
        else:
          v = dat['location']['tz']
          if v:
            tzone = v

    except:
      lat = None
      lon = None
      tzone = None
  
  #loc = loc.replace(' ','_')

  # now do autocomplete lookup to get GPS coordinates
  if lat==None or lon==None:
    try:
      req = urllib2.urlopen('http://autocomplete.wunderground.com/aq?h=0&query='+loc)
      dat = json.load(req)
      if dat['RESULTS']:
        v = dat['RESULTS'][0]['lat']
        if v and isFloat(v):
          lat = v
        v = dat['RESULTS'][0]['lon']
        if v and isFloat(v):
          lon = v
        v = dat['RESULTS'][0]['tz']
        if v:
          tzone = v
        else:
          v = dat['RESULTS'][0]['tz_long']
          if v:
            tzone = v

    except:
      lat = None
      lon = None
      tzone = None
    
  if (lat) and (lon):
    owm_loc = 'lat='+lat+'&lon='+lon
    loc = ''+lat+','+lon
  else:
    owm_loc = 'q='+loc
  
  maxh, minh, meant, pre, pre_today, h_today, sunrise, sunset, scale, toffset = [-1, -1, -500, -1, -1, -1, -1, -1, -1, -1]

  if tzone:
    try:
      tnow = pytz.utc.localize(datetime.utcnow())
      tdelta = tnow.astimezone(pytz.timezone(tzone)).utcoffset()
      toffset = tdelta.days*96+tdelta.seconds/900+48;
    except:
      toffset=-1

  try:
    req = urllib2.urlopen('http://api.openweathermap.org/data/2.5/weather?'+owm_loc)
    dat = json.load(req)
    if dat['sys']:
      v = dat['sys']['sunrise']
      if v:
        sunrise = safe_int(v, sunrise)
      v = dat['sys']['sunset']
      if v:
        sunset = safe_int(v, sunset)
  except:
    pass
        
  try:
    req = urllib2.urlopen('http://api.wunderground.com/api/'+key+'/yesterday/conditions/q/'+loc+'.json')
    dat = json.load(req)

    if dat['history'] and dat['history']['dailysummary']:
      info = dat['history']['dailysummary'][0]
      if info:
        v = info['maxhumidity']
        if v:
          maxh = safe_float(v, maxh)
        v = info['minhumidity']
        if v:
          minh = safe_float(v, minh)
        v = info['meantempi']
        if v:
          meant = safe_float(v, meant)
        v = info['precipi']
        if v:
          pre = safe_float(v, pre)
    info = dat['current_observation']
    if info:
      v = info['precip_today_in']
      if v:
        pre_today = safe_float(v, pre_today)
      v = info['relative_humidity'].replace('%','')
      if v:
        h_today = safe_float(v, h_today)

    # calculate water time scale, per https://github.com/rszimm/sprinklers_pi/blob/master/Weather.cpp
    hf = 0
    if (maxh>=0) and (minh>=0):
      hf = 30 - (maxh+minh)/2
    #elif (h_today>=0):
    #  hf = 30 - h_today
    tf = 0
    if (meant > -500):
      tf = (meant - 70) * 4
    rf = 0
    if (pre>=0):
      rf -= pre * 200
    if (pre_today>=0):
      rf -= pre_today * 200
    scale = (int)(100 + hf + tf + rf)
    if (scale<0):
      scale = 0
    if (scale>200):
      scale = 200
  except:
    pass

  # prepare sunrise sunset time
  delta = 3600/4*(toffset-48)
  if (sunrise >= 0):
    sunrise = int(((sunrise+delta)%86400)/60)
  if (sunset >= 0):
    sunset =  int(((sunset +delta)%86400)/60)
 
  if of=='json' or of=='JSON':
    print '{"scale":%d, "tz":%d, "sunrise":%d, "sunset":%d, "maxh":%d, "minh":%d, "meant":%d, "pre":%f, "prec":%f, "hc":%d}' % (scale, toffset, sunrise, sunset, int(maxh), int(minh), int(meant), pre, pre_today, int(h_today))
  else:
    print '&scale=%d&tz=%d&sunrise=%d&sunset=%d&maxh=%d&minh=%d&meant=%d&pre=%f&prec=%f&hc=%d' % (scale, toffset, sunrise, sunset, int(maxh), int(minh), int(meant), pre, pre_today, int(h_today))
  return

if __name__ == "__main__":
	sys.exit(main())
