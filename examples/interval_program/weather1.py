#!/usr/bin/python
import urllib, urllib2, cgi
import json, datetime, time, sys

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

def main():
  print 'Content-Type: text/html'
  print

  form = cgi.FieldStorage()
  loc = form.getfirst('loc', '')
  key = form.getfirst('key', '')
  tz = form.getfirst('tz','')
  of = form.getfirst('format','')

  if(not loc):
    if of=='json' or of=='JSON':
      print '{"err":"missing_location"}'
    else:
      print '&err=missing_location'
    return

  if(not key):
    if of=='json' or of=='JSON':
      print '{"err":"missing_key"}'
    else:
      print '&err=missing_key'
    return
 
  loc = loc.replace(' ','_')
  if(not tz):
    tz = '48'
  try:
    tz = int(tz)
  except:
    if of=='json' or of=='JSON':
      print '{"err":"missing_tz"}'
    else:
      print '&err=missing_tz'
    return

  maxh, minh, meant, pre, pre_today, h_today, sunrise, sunset = [-1, -1, -500, -1, -1, -1, -1, -1]
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

    req = urllib2.urlopen("http://api.openweathermap.org/data/2.5/weather?q="+loc)
    dat = json.load(req)
    if dat['sys']:
      v = dat['sys']['sunrise']
      if v:
        sunrise = safe_int(v, sunrise)
      v = dat['sys']['sunset']
      if v:
        sunset = safe_int(v, sunset)

  except:
    if of=='json' or of=='JSON':
      print '{"err":"unknown"}'
    else:
      print '&err=unknown'
    return

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

  # prepare sunrise sunset time
  delta = 3600/4*(tz-48)
  if (sunrise >= 0):
    sunrise = int(((sunrise+delta)%86400)/60)
  if (sunset >= 0):
    sunset =  int(((sunset +delta)%86400)/60)
 
  if of=='json' or of=='JSON':
    print '{"scale":%d, "sunrise":%d, "sunset":%d, "maxh":%d, "minh":%d, "meant":%d, "pre":%f, "prec":%f, "hc":%d}' % (scale, sunrise, sunset, int(maxh), int(minh), int(meant), pre, pre_today, int(h_today))
  else:
    print '&scale=%d&sunrise=%d&sunset=%d&maxh=%d&minh=%d&meant=%d&pre=%f&prec=%f&hc=%d' % (scale, sunrise, sunset, int(maxh), int(minh), int(meant), pre, pre_today, int(h_today))
  return

if __name__ == "__main__":
	sys.exit(main())
