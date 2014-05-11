/* rr_types.c - Tables with information for handling
                all rr types known to pdnsd, plus
		some helper functions useful for turning
		binary RR data into text or vice versa.

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2003, 2004, 2007, 2010, 2011 Paul A. Rombouts

  This file is part of the pdnsd package.

  pdnsd is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  pdnsd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pdnsd; see the file COPYING. If not, see
  <http://www.gnu.org/licenses/>.
*/

#include <config.h>
#include <string.h>
#include <stdio.h>
#define DEFINE_RR_TYPE_ARRAYS 1
#include "helpers.h"
#include "dns.h"
#include "rr_types.h"


/*
 * OK, this is inefficient. But it is used _really_ seldom (only in some cases while parsing the
 * config file or by pdnsd-ctl), so it is much more effective to sort by id.
 */
int rr_tp_byname(char *name)
{
	int i;

	for (i=0;i<T_NUM;i++) {
		if (strcmp(name, rrnames[i])==0)
			return i+T_MIN;
	}
	return -1; /* invalid */
}

/* The following is not needed by pdnsd-ctl. */
#ifndef CLIENT_ONLY

static const unsigned int poweroften[8] = {1, 10, 100, 1000, 10000, 100000,
					   1000000,10000000};
#define NPRECSIZE (sizeof "90000000")
/* takes an XeY precision/size value, returns a string representation.
   This is an adapted version of the function of the same name that
   can be found in the BIND 9 source.
 */
static const char *precsize_ntoa(uint8_t prec,char *retbuf)
{
	unsigned int mantissa, exponent;

	mantissa = (prec >> 4);
	exponent = (prec & 0x0f);

	if(mantissa>=10 || exponent>=10)
		return NULL;
	if (exponent>= 2)
		sprintf(retbuf, "%u", mantissa * poweroften[exponent-2]);
	else
		sprintf(retbuf, "0.%.2u", mantissa * poweroften[exponent]);
	return (retbuf);
}

/* takes an on-the-wire LOC RR and formats it in a human readable format.
   This is an adapted version of the loc_ntoa function that
   can be found in the BIND 9 source.
 */
const char *loc2str(const void *binary, char *ascii, size_t asclen)
{
	const unsigned char *cp = binary;

	int latdeg, latmin, latsec, latsecfrac;
	int longdeg, longmin, longsec, longsecfrac;
	char northsouth, eastwest;
	const char *altsign;
	int altmeters, altfrac;

	const uint32_t referencealt = 100000 * 100;

	int32_t latval, longval, altval;
	uint32_t templ;
	uint8_t sizeval, hpval, vpval, versionval;

	char sizestr[NPRECSIZE],hpstr[NPRECSIZE],vpstr[NPRECSIZE];

	versionval = *cp++;

	if (versionval) {
		/* unknown LOC RR version */
		return NULL;
	}

	sizeval = *cp++;

	hpval = *cp++;
	vpval = *cp++;

	GETINT32(templ, cp);
	latval = (templ - ((unsigned)1<<31));

	GETINT32(templ, cp);
	longval = (templ - ((unsigned)1<<31));

	GETINT32(templ, cp);
	if (templ < referencealt) { /* below WGS 84 spheroid */
		altval = referencealt - templ;
		altsign = "-";
	} else {
		altval = templ - referencealt;
		altsign = "";
	}

	if (latval < 0) {
		northsouth = 'S';
		latval = -latval;
	} else
		northsouth = 'N';

	latsecfrac = latval % 1000;
	latval /= 1000;
	latsec = latval % 60;
	latval /= 60;
	latmin = latval % 60;
	latval /= 60;
	latdeg = latval;

	if (longval < 0) {
		eastwest = 'W';
		longval = -longval;
	} else
		eastwest = 'E';

	longsecfrac = longval % 1000;
	longval /= 1000;
	longsec = longval % 60;
	longval /= 60;
	longmin = longval % 60;
	longval /= 60;
	longdeg = longval;

	altfrac = altval % 100;
	altmeters = (altval / 100);

	if(!precsize_ntoa(sizeval,sizestr) || !precsize_ntoa(hpval,hpstr) || !precsize_ntoa(vpval,vpstr))
		return NULL;
	{
		int n=snprintf(ascii,asclen,
			       "%d %.2d %.2d.%.3d %c %d %.2d %.2d.%.3d %c %s%d.%.2dm %sm %sm %sm",
			       latdeg, latmin, latsec, latsecfrac, northsouth,
			       longdeg, longmin, longsec, longsecfrac, eastwest,
			       altsign, altmeters, altfrac,
			       sizestr, hpstr, vpstr);
		if(n<0 || n>=asclen)
			return NULL;
	}

	return (ascii);
}

#endif
