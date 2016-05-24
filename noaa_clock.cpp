/**************************************************************************	\
*
* PROGRAM       noaa_clock.cpp
*
* DESCRIPTION   Solar time / wall-clock time / twilight clock
*
* ARGUMENTS     (See usage ().)
*
* GLOBALS       (See global variable block.)
*
* RETURNS       Exit value
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         Linked with Qt 5.4.
*
*               Designed for Full HD display size. Adjust dimensions for
*               your own needs.
*
\**************************************************************************/

#include "stdafx.h"
#include <Windows.h>
#include <time.h>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QMetaObject>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qmainwindow.h>
#include <QtGui/QtGUi>
#include <QtGui/QPainter>

#define OX   500          // Time display origin X coordinate
#define OY   500          // Time display origin Y coordinate
#define EOX 1100          // Sun elevation display origin X coordinate
#define AOX 1500          // Sun azimuth display origin X coordinate
#define AOY  (OY - 250)   // Sun azimuth display origin Y coordinate
#define LOX 1500          // Sun longitude display origin X coordinate
#define LOY  (OY + 250)   // Sun longitude display origin Y coordinate



class DispWidget : public QWidget {
  Q_OBJECT
  public:
                DispWidget  (void) {}
                ~DispWidget (void) {}
  void          upd         (void);
  void          drawtl      (float cf, int startl, int endl, unsigned int c);
  void          drawnum     (float cf, int n, int loc, unsigned int c);
  void          paintEvent  (QPaintEvent *e);
  QImage        *img;
  unsigned char *imgdata;
  public slots:
  void          eloop       (void);
  };



#include "noaa_clock.moc"



// Global variables

QDate      d;
QTime      t;
char       line     [1024];   // File line buffer
float      solarmin [1440],   // Solar time minute
           elev     [1440],   // Sun elevation
           elevc    [1440],   // Corrected Sun elevation
           azim     [1440],   // Sun azimuth
           sunlong  [1440];   // Sun longitude
DispWidget *dw = NULL;        // DIsplay widget
QPainter   *painter;          // Qt painter object
double     dpi = 2.0 * 3.1415926535897932;
float      cf,                // Current time display circle frction
           cfs,               // Current fraction sine
           cfc,               // Current fraction cosine
           caz,               // Current Sun azimuth
           ce,                // Current Sun elevation
           cec,               // Current corrected Sun elevation
           csl,               // Current Sun latitude
           idx;               // Solar time minute to access the per-minute tables

// Global NOAA variables

double lat_deg, 
       long_deg,
       date_d,
       wtime_day,
       dst_hr,
       timezone_hr;
double my_timezone;    // TImezone of user's own location [Hours]
double jday,           // Julian day
       jcen,           // Julian century
       gmlong_deg,     // Sun geom mean longitude
       gmanom_deg,     // Sun geom mean anomaly
       eccent,         // Earth orbit eccentricity
       eqofctr,        // Sun eq of ctr
       truelong_deg,   // Sun true longitude
       trueanom_deg,   // Sun true anomaly
       radvect_au,
       applong_deg,
       moe_deg,        // Mean obliq ecliptic
       ocorr_deg,      // Obliq corr
       rtasc_deg,      // Sun right ascension
       decl_deg,       // Sun declination
       var_y,
       eqoftime_min,   // Equation of time
       ha_rise_deg,
       noon_lst,
       rise_lst,
       set_lst,
       lightdur_min,
       soltime_min,
       hrangle_deg,
       zangle_deg,      // Zenith angle
       elev_deg,        // Sun elevation
       refract_deg,     // Atmospheric refraction
       elevc_deg,       // Refraction-corrected sun elevation
       az_deg;          // Sun azimuth



/**************************************************************************\
*
* FUNCTION      d2r
*
* DESCRIPTION   Degrees to radians.
*
* ARGUMENTS     d   Degrees
*
* GLOBALS       dpi   2 * pi
*
* RETURNS       Radians
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

double d2r (double d) {return (dpi * d / 360.0);}



/**************************************************************************\
*
* FUNCTION      r2d
*
* DESCRIPTION   Radians to degrees.
*
* ARGUMENTS     r   Radians
*
* GLOBALS       dpi   2 * pi
*
* RETURNS       Degrees
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

double r2d (double r) {return (360.0 * r / dpi);}



/**************************************************************************\
*
* METHOD        DispWidget :: drawtl
*
* DESCRIPTION   Time display radial lines, scale and pointer.
*
* ARGUMENTS     f        Fraction of a full circle
*               startl   Starting radius
*               endl     Ending radius
*               c        Color
*
* GLOBALS       dpi   2 * pi
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

void DispWidget :: drawtl (float f, int startl, int endl, unsigned int c) {
  cfs =   sin (dpi * (- f + 0.25));
  cfc = - cos (dpi * (- f + 0.25));
  painter -> setPen (QColor (c));
  painter -> drawLine (OX + startl * cfc, OY + startl * cfs, OX + endl * cfc, OY + endl * cfs);
  }



/**************************************************************************\
*
* METHOD        DispWidget :: drawnum
*
* DESCRIPTION   Time display number drawing.
*
* ARGUMENTS     f     Fraction of a full circle
*               n     Number to draw
*               loc   Radius of placement
*               c     Color
*
* GLOBALS       dpi   2 * pi
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

void DispWidget :: drawnum (float f, int n, int loc, unsigned int c) {
  float   fs, fc;
  QString str;
  char    s [4];
  sprintf (s, "%d", n);
  str = QString (s);
  fs =   sin (dpi * (- f + 0.25));
  fc = - cos (dpi * (- f + 0.25));
  painter -> setPen (QColor (c));
  painter -> drawText (OX + loc * fc - 8, OY + loc * fs, 16, 10, Qt :: AlignCenter, str);
  }


/**************************************************************************\
*
* METHOD        DispWidget :: upd
*
* DESCRIPTION   Display updating.
*
* ARGUMENTS     -
*
* GLOBALS       dpi        2 * pi
*               solarmin   Solar time in minutes
*               elevc      Corrected elevation of Sun
*               cf         Current time display circle fraction
*               ce         Current elevation of Sun
*               cec        Current corrected elevation of Sun
*               caz        Current azimuth of Sun
*               csl        Current longitude of Sun
~               painter    Qt painter object
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

void DispWidget :: upd (void) {

  QString str;
  char    s [32];
  int     i, j, e, ec;
  float   f, as, ac;

  // Time display, color zones (i and j before addition are local for the tables, solar for the display)

  for (j = 0 ; j < 5760 ; j++) {
    i = j / 4;
    i = i - solarmin [0]; if (i < 0) i += 1440; if (i >= 1440) i -= 1440;
    f = j / 5760.0;
    if  (elevc [i] >=  3.0)                          drawtl (f, 310, 315, 0x00ffffff);
    if ((elevc [i] <   3.0) && (elevc [i] >=   0.0)) drawtl (f, 310, 315, 0x00ffff00);
    if ((elevc [i] <   0.0) && (elevc [i] >=  -6.0)) drawtl (f, 310, 315, 0x00ff0000);
    if ((elevc [i] <  -6.0) && (elevc [i] >= -12.0)) drawtl (f, 310, 315, 0x000000ff);
    if ((elevc [i] < -12.0) && (elevc [i] >= -18.0)) drawtl (f, 310, 315, 0x00808080);
    }

  // Time display, solar time

  for (i = 0 ; i < 1440 ; i++) {
    f = i / 1440.0;
    if      (i % 360 == 0) drawtl (f, 420, 470, 0x00ffffff);
    if      (i %  60 == 0) drawtl (f, 420, 450, 0x00ffffff);
    else if (i %  20 == 0) drawtl (f, 420, 430, 0x00ffffff);
    }

  // Time display, wall clock time

  for (i = 0 ; i < 1440 ; i++) {
    f = (i + solarmin [0]) / 1440.0;
    if      (i % 360 == 0) drawtl  (f, 360,    410, 0x00ffffff);
    if      (i %  60 == 0) drawtl  (f, 360,    390, 0x00ffffff);
    else if (i %  20 == 0) drawtl  (f, 360,    370, 0x00ffffff);
    if      (i % 180 == 0) drawnum (f, i / 60, 340, 0x00ffffff);
    }

  // Time display, pointer

  drawtl (cf, 0, 425, 0x00ff8000);

  // Sun elevation

  sprintf (s, "Sun elevation");
  str = QString (s);
  painter -> setPen (QColor (255, 255, 255));
  painter -> drawText (EOX - 50, 10, 104, 10, Qt :: AlignCenter, str);
  painter -> setPen (QColor (255, 255, 255));
  for (i = EOX - 3 ; i <= EOX ; i++)
    painter -> drawLine (i, OY - 450, i, OY);
  for (i = -90 ; i <= 90 ; i++) {
    sprintf (s, "%+d", i);
    str = QString (s);
    painter -> setPen (QColor (255, 255, 255));
    painter -> drawLine (EOX, OY - 5 * i, EOX + 3, OY - 5 * i);
    if ((i %  5) == 0) painter -> drawLine (EOX, OY - 5 * i, EOX + 6, OY - 5 * i);
    if ((i % 10) == 0) painter -> drawText (EOX + 10, OY - 5 * i - 8, 24, 10, Qt :: AlignCenter, str);
    }
  for (i = EOX - 3 ; i <= EOX ; i++) {
    painter -> setPen (QColor (255, 255,   0)); painter -> drawLine (i, OY - 5 * ( +3), i, OY - 5 * ( +0));
    painter -> setPen (QColor (255,   0,   0)); painter -> drawLine (i, OY - 5 * ( +0), i, OY - 5 * ( -6));
    painter -> setPen (QColor (  0,   0, 255)); painter -> drawLine (i, OY - 5 * ( -6), i, OY - 5 * (-12));
    painter -> setPen (QColor (128, 128, 128)); painter -> drawLine (i, OY - 5 * (-12), i, OY - 5 * (-18));
    }
  for (i = -2 ; i <= 2 ; i++) {
    ec = OY - 5 * cec ; painter -> setPen (QColor (255, 192,   0));
    painter -> drawLine (EOX - 20, ec + i, EOX - 8 * abs (i), ec + i);
    }
  for (i = -2 ; i <= 2 ; i++) {
    e  = OY - 5 * ce;  painter -> setPen (QColor (192, 192, 192));
    painter -> drawLine (EOX - 20, e,  EOX - 8 * abs (i), e);
    }
  sprintf (s, "%+5.1f", cec);
  str = QString (s);
  painter -> setPen (QColor (255, 255, 255));
  painter -> drawText (EOX - 60, ec - 4, 40, 10, Qt :: AlignCenter, str);
  if (fabs ((double) (cec - ce)) > 0.1) {
    sprintf (s, "%+5.1f", ce);
    str = QString (s);
    painter -> setPen (QColor (128, 128, 128));
    painter -> drawText (EOX + 30, e - 4, 40, 10, Qt :: AlignCenter, str);
    }

  // Sun azimuth

  sprintf (s, "Sun azimuth");
  str = QString (s);
  painter -> setPen (QColor (255, 255, 255));
  painter -> drawText (AOX - 40, 10, 88, 10, Qt :: AlignCenter, str);
  painter -> setPen (QColor (255, 255, 255));
  painter -> drawEllipse (AOX - 200, AOY - 200, 400, 400);
  painter -> setPen (QColor (255, 128,   0));
  as = - sin (dpi * (caz - 90.0) / 360.0);
  ac = - cos (dpi * (caz - 90.0) / 360.0);
  painter -> drawLine (AOX, AOY,  AOX + 200 * ac, AOY + 200 * as);
  sprintf (s, "%3.0f", caz);
  str = QString (s);
  painter -> setPen (QColor (255, 255, 255));
  painter -> drawText (AOX + 220 * ac - 12, AOY + 220 * as - 8, 24, 10, Qt :: AlignCenter, str);

  // Sun longitude

  sprintf (s, "Sun longitude");
  str = QString (s);
  painter -> setPen (QColor (255, 255, 255));
  painter -> drawText (LOX - 60, OY + 10, 104, 14, Qt :: AlignCenter, str);
  painter -> setPen (QColor (255, 255, 255));
  painter -> drawEllipse (LOX - 200, LOY - 200, 400, 400);
  painter -> setPen (QColor (255, 128,   0));
  as = - sin (dpi * csl / 360.0);
  ac = - cos (dpi * csl / 360.0);
  painter -> drawLine (LOX, LOY,  LOX + 200 * ac, LOY + 200 * as);
  sprintf (s, "%3.0f", csl);
  str = QString (s);
  painter -> setPen (QColor (255, 255, 255));
  painter -> drawText (LOX + 220 * ac - 12, LOY + 220 * as - 8, 24, 10, Qt :: AlignCenter, str);

  }



/**************************************************************************\
*
* METHOD        DispWidget :: paintEvent
*
* DESCRIPTION   Display painting callback.
*
* ARGUMENTS     e   Paint event
*
* GLOBALS       painter   Qt painter object
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

void DispWidget :: paintEvent (QPaintEvent *e) {
  QRect r (0, 0, 1920, 1080);
  painter -> begin (this);
  painter -> fillRect (0, 0, 1919, 1079, QColor (0, 0, 0));
  upd ();
  painter -> end ();
  }



/**************************************************************************\
*
* FUNCTION      noaa_eq
*
* DESCRIPTION   NOAA solar euations.
*
* ARGUMENTS     -
*
* GLOBALS       (See Global variable file section)
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

void noaa_eq (void) {
  jday = date_d + 2415018.5 + wtime_day - timezone_hr / 24;
  jcen = (jday - 2451545) / 36525;
  gmlong_deg   = fmod (280.46646 + jcen * (36000.76983 + jcen * 0.0003032), 360.0);
  gmanom_deg   = 357.52911 + jcen * (35999.05029 - 0.0001537 * jcen);
  eccent       = 0.016708634 - jcen  * (0.000042037 + 0.0000001267 * jcen);
  eqofctr      =   sin (d2r (gmanom_deg)) * (1.914602 - jcen * (0.004817 + 0.000014 * jcen))
                 + sin (d2r (2 * gmanom_deg)) * (0.019993 - 0.000101 * jcen)
                 + sin (d2r (3 * gmanom_deg)) * 0.000289;
  truelong_deg = gmlong_deg + eqofctr;
  trueanom_deg = gmanom_deg + eqofctr;
  radvect_au   = (1.000001018 * (1 - eccent * eccent)) / (1 + eccent * cos (d2r (trueanom_deg)));
  applong_deg  = truelong_deg - 0.00569 - 0.00478 * sin (d2r (125.04 - 1934.136 * jcen));
  moe_deg      = 23 + (26 + ((21.448 - jcen * (46.815 + jcen * (0.00059 - jcen * 0.001813)))) / 60) / 60;
  ocorr_deg    = moe_deg + 0.00256 * cos (d2r (125.04 - 1934.136 * jcen));
  rtasc_deg    = r2d (atan2 (cos (d2r (applong_deg)), cos (d2r (ocorr_deg)) * sin (d2r (applong_deg))));
  decl_deg     = r2d (asin (sin (d2r (ocorr_deg)) * sin (d2r (applong_deg))));
  var_y        = tan (d2r (ocorr_deg / 2)) * tan (d2r (ocorr_deg / 2));
  eqoftime_min =   4 * r2d (var_y * sin (2 *d2r (gmlong_deg))
                 - 2 * eccent * sin (d2r (gmanom_deg))
                 + 4 * eccent * var_y * sin (d2r (gmanom_deg)) * cos (2 * d2r (gmlong_deg))
                 - 0.5 * var_y * var_y * sin (4 * d2r (gmlong_deg))
                 - 1.25 * eccent * eccent * sin (2 * d2r (gmanom_deg)));
  ha_rise_deg  = r2d (acos (cos (d2r (90.833)) / (cos (d2r (lat_deg)) * cos (d2r (decl_deg))) - tan (d2r (lat_deg)) * tan (d2r (decl_deg))));
  noon_lst     = (720 - 4 * long_deg  - eqoftime_min + timezone_hr * 60) / 1440;
  rise_lst     = noon_lst - ha_rise_deg * 4 / 1440;
  set_lst      = noon_lst + ha_rise_deg * 4 / 1440;
  lightdur_min = 8 * ha_rise_deg;
  soltime_min  = fmod ((wtime_day * 1440 + eqoftime_min + 4 * long_deg - 60 * timezone_hr), 1440.0);
  hrangle_deg  = (soltime_min / 4 < 0) ? (soltime_min / 4 + 180) : (soltime_min / 4 - 180);
  zangle_deg   = r2d (acos (sin (d2r (lat_deg)) * sin (d2r (decl_deg)) + cos (d2r (lat_deg)) * cos (d2r (decl_deg)) * cos (d2r (hrangle_deg))));
  elev_deg         = 90 - zangle_deg;
  if (elev_deg > 85) refract_deg = 0;
  else {
    if (elev_deg > 5) refract_deg = 58.1 / tan (d2r (elev_deg)) - 0.07 / pow (tan (d2r (elev_deg)), 3) + 0.000086 / pow (tan (d2r (elev_deg)), 5);
    else {
      if (elev_deg > -0.575) refract_deg = 1735 + elev_deg * (-518.2 + elev_deg * (103.4 + elev_deg * (-12.79 + elev_deg * 0.711)));
      else                   refract_deg = -20.772 / tan (d2r (elev_deg));
      }
    }
  refract_deg /= 3600;
  elevc_deg = elev_deg + refract_deg;
  if (hrangle_deg > 0)
    az_deg = fmod ((r2d (acos (((sin (d2r (lat_deg)) * cos (d2r (zangle_deg))) - sin (d2r (decl_deg))) / (cos (d2r (lat_deg)) * sin (d2r (zangle_deg))))) + 180), 360.0);
  else
    az_deg = fmod ((540 - r2d (acos (((sin (d2r (lat_deg)) * cos (d2r (zangle_deg))) - sin (d2r (decl_deg))) / (cos (d2r (lat_deg)) * sin (d2r (zangle_deg)))))), 360.0);
  }



/**************************************************************************\
*
* FUNCTION      load
*
* DESCRIPTION   Saving NOAA equation results into per-minute tables.
*
* ARGUMENTS     -
*
* GLOBALS        wtime_day
*                soltime_min    Solar time in minutes
*                elev_deg       Elevation of Sun
*                elevc_deg      Corrected elevation of Sun
*                az_deg         Azimuth of Sun
*                truelong_deg   Longitude of Sun
*                solarmin       Solar time in minutes
*                elev           Elevation of Sun
*                elevc          Corrected elevation of Sun
*                azim           Azimuth of Sun
*                sunlong        Longitude of Sun
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

void load (void) {
  int  i;
  for (i = 0 ; i < 1440 ; i++) {
    wtime_day = (double) i / 1440.0;
    noaa_eq ();
    solarmin [i] = soltime_min;
    elev     [i] = elev_deg;
    elevc    [i] = elevc_deg;
    azim     [i] = az_deg;
    sunlong  [i] = truelong_deg;
    }
  }



/**************************************************************************\
*
* METHOD        DispWidget :: eloop
*
* DESCRIPTION   Display update timer response method.
*
* ARGUMENTS     -
*
* GLOBALS       d
*               date_d
*               t
*               wtime_day
*               idx
*               timezone_hr
*               my_timezone
*               solarmin      Solar time in minutes
*               elevc         Elevation of Sun
*               elevc         Corrected elevation of Sun
*               azim          Azimuth of Sun
*               sunlong       Longitude of Sun
*               cf            Current time display circle fraction
*               cfs           Current fraction sine
*               cfc           Current fraction cosine
*               ce            Current elevation of Sun
*               cec           Current corrected elevation of Sun
*               caz           Current azimuth of Sun
*               csl           Current longitude of Sun
*               dw            Display widget
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

void DispWidget :: eloop (void) {
  QDate dd;
  int   idxw;
  load ();
  d = QDate :: currentDate ();
  dd = QDate (1900, 1, 1);
  date_d = dd.daysTo (d) + 2;
  t = QTime :: currentTime ();
  t = t.addSecs (3600 * (timezone_hr - my_timezone));
  idxw = 60 * t.hour () + t.minute ();
  wtime_day = idxw / 1440.0;
  idx = solarmin [(int) (60 * t.hour () + t.minute ())];
  cf = idx / 1440.0;
  cfs = sin (dpi * (- cf + 0.25));
  cfc = cos (dpi * (- cf + 0.25));
  caz = azim    [(int) idxw];
  ce  = elev    [(int) idxw];
  cec = elevc   [(int) idxw];
  csl = sunlong [(int) idxw];
  dw -> repaint ();
  }



/**************************************************************************\
*
* FUNCTION      setdst_eu
*
* DESCRIPTION   European Union daylight saving time adjustment.
*
* ARGUMENTS     -
*
* GLOBALS        wtime_day
*                soltime_min    Solar time in minutes
*                elev_deg       Elevation of Sun
*                elevc_deg      Corrected elevation of Sun
*                az_deg         Azimuth of Sun
*                truelong_deg   Longitude of Sun
*                solarmin       Solar time in minutes
*                elev           Elevation of Sun
*                elevc          Corrected elevation of Sun
*                azim           Azimuth of Sun
*                sunlong        Longitude of Sun
*
* RETURNS       Offset in hours
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

int setdst_eu (void) {
  QDate d, dmar, doct;
  QTime t;
  int   n;
  d = QDate :: currentDate ();
  t = QTime :: currentTime ();
  t = t.addSecs (3600 * (timezone_hr - my_timezone));
  n = 31;
  dmar = QDate (d.year (),  3, n);
  while (dmar.dayOfWeek () != 7) {n--; dmar = QDate (d.year (),  3, n);}
  n = 31;
  doct = QDate (d.year (), 10, 31);
  while (doct.dayOfWeek () != 7) {n--; doct = QDate (d.year (),  10, n);}
  n = 0;
  if ((d == dmar) && (t.hour () > 3)) n = 1;
  if (d > dmar) n = 1;
  if ((d == doct) && (t.hour () > 3)) n = 0;
  if (d > doct) n = 0;
  return (n);
  }



/**************************************************************************\
*
* FUNCTION      showlocs
*
* DESCRIPTION   Listing of locations in the configuration file.
*
* ARGUMENTS     -
*
* GLOBALS       -
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

void showlocs (void) {
  FILE *f;
  char line [256];
  f = fopen ("noaa_clock.cnf", "r");
  if (f) {
    char rloc [256];
    printf ("Currently known locations are: ");
    while (! feof (f)) {
      fgets (line, 252, f);
      if (strchr (line, '*')) break;
      sscanf (line, "%s", rloc);
      printf ("%s ", rloc);
      }
    }
  printf ("\n\n");
  fclose (f);
  }



/**************************************************************************\
*
* FUNCTION      setcoord
*
* DESCRIPTION   Location to coordinate pair.
*
* ARGUMENTS      loc   Location name
*                la    Latitude [Decimal degrees]
*                lo    Longitude [Decimal degrees]
*                tz    Timezone [Hours]
*
* GLOBALS       -
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

bool setcoord (char *loc, double *la, double *lo, double *tz) {

  FILE *f;
  char line [256];
  int  dst = 0;

  // Load geographic configuration file, if it exists

  f = fopen ("noaa_clock.cnf", "r");
  if (f) {
    char dststr [256], rloc [256];
    while (! feof (f)) {
      fgets (line, 252, f);
      if (feof (f)) break;
      if (line [0] == '\n') continue;
      if (strchr (line, '*')) break;
      sscanf (line, "%s %lf %lf %lf %s", rloc, la, lo, tz, dststr);
      if (strcmpi (rloc, loc) == 0) {
        if (strcmpi (dststr, "EU") == 0) *tz += setdst_eu ();
        break;
        }
      }
    }
  if (f) {fclose (f); return (true);}

  // Fallback for a missing noaa_clock.cnf

  if (strcmpi (loc, "Helsinki") == 0) {
    *la = 60.16; *lo = 24.83; *tz = 2;
    *tz += setdst_eu ();
    return (true);
    }
  if (strcmpi (loc, "Riihimäki") == 0) {
    *la = 60.739; *lo = 24.772; *tz = 2;
    *tz += setdst_eu ();
    return (true);
    }
  if (strcmpi (loc, "Tampere") == 0) {
    *la = 61.498; *lo = 23.761; *tz = 2;
    *tz += setdst_eu ();
    return (true);
    }
  if (strcmpi (loc, "Ylöjärvi") == 0) {
    *la = 61.55; *lo = 23.583; *tz = 2;
    *tz += setdst_eu ();
    return (true);
    }
  if (strcmpi (loc, "Rovaniemi") == 0) {
    *la = 66.5; *lo = 25.733; *tz = 2;
    *tz += setdst_eu ();
    return (true);
    }
  if (strcmpi (loc, "Inari") == 0) {
    *la = 68.905; *lo = 27.03; *tz = 2;
    *tz += setdst_eu ();
    return (true);
    }
  if (strcmpi (loc, "Utsjoki") == 0) {
    *la = 69.9; *lo = 27.017; *tz = 2;
    *tz += setdst_eu ();
    return (true);
    }
  if ((strcmpi (loc, "Tukholma") == 0) || (strcmpi (loc, "Stockholm") == 0)) {
    *la = 59.329; *lo = 18.069; *tz = 1;
    *tz += setdst_eu ();
    return (true);
    }
  if (strcmpi (loc, "Vargön") == 0) {
    *la = 58.35; *lo = 12.4; *tz = 1;
    *tz += setdst_eu ();
    return (true);
    }
  if (strcmpi (loc, "Reykjavik") == 0) {
    *la = 64.135; *lo = -21.895; *tz = 0;   // No DST
    return (true);
    }
  if (strcmpi (loc, "Longyearbyen") == 0) {
    *la = 78.22; *lo = 15.65; *tz = 1;
    *tz += setdst_eu ();
    return (true);
    }
  if ((strcmpi (loc, "Tallinna") == 0) || (strcmpi (loc, "Tallinn") == 0)) {
    *la = 59.437; *lo = 24.745; *tz = 2;
    *tz += setdst_eu ();
    return (true);
    }
  if ((strcmpi (loc, "Moskova") == 0) || (strcmpi (loc, "Moscow") == 0)) {
    *la = 55.75; *lo = 37.617; *tz = 2;
    *tz += setdst_eu ();
    return (true);
    }
  if ((strcmpi (loc, "Lontoo") == 0) || (strcmpi (loc, "London") == 0)) {
    *la = 51.5; *lo = -0.126; *tz = 0;
    *tz += setdst_eu ();
    return (true);
    }
  if ((strcmpi (loc, "Hampuri") == 0) || (strcmpi (loc, "Hamburg") == 0)) {
    *la = 53.553; *lo = 9.992; *tz = 1;
    *tz += setdst_eu ();
    return (true);
    }
  if ((strcmpi (loc, "Rooma") == 0) || (strcmpi (loc, "Roma") == 0)) {
    *la = 41.895; *lo = 12.482; *tz = 1;
    *tz += setdst_eu ();
    return (true);
    }
  if ((strcmpi (loc, "Tokio") == 0) || (strcmpi (loc, "Tokyo") == 0)) {
    *la = 35.683; *lo = 139.767; *tz = 9;   // No DST
    return (true);
    }
  if ((strcmpi (loc, "Teheran") == 0) || (strcmpi (loc, "Tehran") == 0)) {
    *la = 35.696; *lo = 51.423; *tz = 3.5;   // No DST
    return (true);
    }

  return (false);
 
 }



/**************************************************************************\
*
* FUNCTION      usage
*
* DESCRIPTION   Invoking directions.
*
* ARGUMENTS     pn    Program name
*
* GLOBALS       -
*
* RETURNS       -
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

void usage (char *pn) {
  printf ("Use: %s latitude longitude timezone [mytimezone]\n", pn);
  printf ("Or:  %s locationname [mytimezone]\n\n", pn);
  printf ("Longitude is positive east, timezone must include the daylight saving time.\n\n");
  }



/**************************************************************************\
*
* FUNCTION      main
*
* DESCRIPTION   Main function, the application entry point.
*
* ARGUMENTS     argc   Argument count
*               arg    Argument vector
*
* GLOBALS       lat_deg       Latitude [Decimal degrees]
*               long_deg      Longitude [Decimal degrees]
*               timezone_hr   Timezone [Hours]
*               my_timezone   Timezone of the user's location [Hours]
*               dw            Display widget
*               painter       Qt painter object
*
* RETURNS       Error code
*
* HISTORY       2016 05 24   JPT   File documenting begins
*
* NOTES         -
*
\**************************************************************************/

int main (int argc, char *argv []) {
  QTimer timer;
  char   loc [256];
  if (argc == 2) {
    sscanf (argv [1], "%s", loc);
    if (setcoord (loc, &lat_deg, &long_deg, &timezone_hr) == false) {
      printf ("'%s' is an unknown location.\n\n", loc);
      usage (argv [0]);
      showlocs ();
      return (1);
      }
    my_timezone = timezone_hr;
    }
  else if (argc == 3) {
    sscanf (argv [1], "%s", loc);
    sscanf (argv [2], "%lf", &my_timezone);
    if (setcoord (loc, &lat_deg, &long_deg, &timezone_hr) == false) {
      printf ("'%s' is an unknown location.\n\n", loc);
      usage (argv [0]);
      showlocs ();
      return (1);
      }
    }
  else if (argc == 4) {
    sscanf (argv [1], "%lf", &lat_deg);
    sscanf (argv [2], "%lf", &long_deg);
    sscanf (argv [3], "%lf", &timezone_hr);
    my_timezone = timezone_hr;
    }
  else if (argc >= 5) {
    sscanf (argv [1], "%lf", &lat_deg);
    sscanf (argv [2], "%lf", &long_deg);
    sscanf (argv [3], "%lf", &timezone_hr);
    sscanf (argv [4], "%lf", &my_timezone);
    }
  else {
    usage (argv [0]);
    return (1);
    }
  QApplication app (argc, NULL);
  dw = new DispWidget ();
  dw -> resize (1920, 1080);
  dw -> show ();
  painter = new QPainter ();
  dw -> eloop ();
  timer.start (5000);
  QObject :: connect (&timer, SIGNAL (timeout ()), dw, SLOT (eloop ()));
  dw -> eloop ();   // Extra iteration
  app.exec ();
  return (0);
  }
