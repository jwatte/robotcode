%
M03
G97S5100
G20
G17
G94
F5

(left side)
G00Z0.1
G00X1.125Y0
G01Z-0.2F3
(left diagonal near to far)
G01X0.7830Y1.0F05
G01Z0.1
G00X2.125Y0
G01Z-0.2F3
(right diagonal near to far)
G01X1.7830Y1.0F05
(right to left, top)
G01X0.7830F03
G01Z0.1
(right to left, bottom)
G002.125Y0
G01Z-0.2F3
G01X1.125F03
G01Z0.1F05

(right side)
G00X7.785Y0
G01Z-0.2F3
(left diagonal near to far)
G01X8.2170Y1.0F05
G01Z0.1
G00X8.875Y0
G01Z-0.2F3
(right diagonal near to far)
G01X9.2170Y1.0F05
(right to left, top)
G01X8.2170F03
G01Z0.1
(right to left, bottom)
G008.875Y0
G01Z-0.2F3
G01X7.875F03
G01Z0.1

G00Z1
G00X0Y0

M05
M30

