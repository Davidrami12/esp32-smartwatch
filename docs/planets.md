# Planets

Separate APPS → PLANETS app: a static heliocentric top-down diagram, viewed
from north of the ecliptic, J2000 equinox to the right.
Earth has a white outline. No network requests or animation. Only the title
and BACK remain; the date and lower captions were removed to enlarge the map.
All eight names are always visible beside their planets in matching colors;
there is no tap-to-select interaction. A bounded placement search keeps names
inside the map and reduces overlap with other labels and planet dots.

Planet longitudes use JPL SSD approximate Keplerian elements and rates,
Table 1, with a bounded Newton solution of Kepler's equation and projection
onto the J2000 ecliptic. Earth uses the Earth/Moon barycentre approximation.
UTC approximates TDB; this is educational, not navigation-grade astronomy.
Supported app dates: 2000–2050. Invalid clocks hide the map's planets.

Orbit rings are schematic circles with compressed spacing; neither distances
nor planet sizes are to scale. Positions are heliocentric, not a Madrid sky
visibility chart. Recalculated on entry, wake or clock validity recovery.
The detailed elliptical-orbit/zoom version was reverted for responsiveness.
Rendering uses eight native arcs rather than 1,280 individual line segments.

Formulae and coefficients:
https://ssd.jpl.nasa.gov/planets/approx_pos.html
E.M. Standish and J.G. Williams, approximate positions (JPL SSD).
