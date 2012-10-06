import math

def printarc(lx = 1.125, d = 1.0, z = 0.35):
    r = d / 2.0

    # position tool
    print "G00X%.4fZ0.1" % (lx,)

    # straight cuts to reach depth
    print "(clear depth)"
    zz = 0.1
    while zz < z:
        print "G01X%.4fZ%.1fF03" % (lx, -zz)
        print "G01X%.4fF03" % (lx + d)
        print "G00X%.4f" % (lx,)
        zz += 0.1


    print "(rough curve)"
    # straight cuts in shrinking radius to clear material
    for i in (0, 8, 16, 25, 37, 50, 65, 80):
        f = i * math.pi / 180
        print "G01X%.4fZ%.4fF03" % (lx + r - math.cos(f) * r, -z - math.sin(f) * r)
        print "G01X%.4fF03" % (lx + r + math.cos(f) * r,)
        print "G00X%.4f" % (lx + r - math.cos(f) * r,)

    print "(finish arc)"
    # arc cut to round out
    print "G18"
    print "G91.1"
    print "G00X%.4fZ%.4fF03" % (lx, -z)
    print "G02I%.4fX%.4fF03" % (r, lx + d)
    print "G01Z0.1F05"
    print ""

print "(front left)"
printarc(1.125)
print "(front right)"
printarc(7.875)
#print "(rear left)"
#printarc(1.125 - 0.3420)
#print "(rear right)"
#printarc(7.875 + 0.3420)
