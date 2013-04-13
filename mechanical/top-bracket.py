
import milling

width = 32
height = 32
depth = 6
blockPad = 4

diameter = 12.7
direction = milling.CONVENTIONAL

r0 = milling.rect(width/-2 - diameter/2, height/-2 - diameter/2, width + diameter, height + diameter)
r1 = milling.rect(width/-2 + blockPad + diameter/2, height/-2 - diameter/2, width - blockPad * 2 - diameter, height + diameter)
r2 = milling.rect(width/-2 - diameter/2, height/-2 + diameter/2 + blockPad, diameter/2 + blockPad, height - diameter - blockPad * 2)
r3 = milling.rect(width/2 - diameter/2 - blockPad, height/-2 + diameter/2 + blockPad, diameter/2 + blockPad, height - diameter - blockPad / 2)

m = milling.start()
t = m.tool(diameter, 3000, 250, 100)
m.useTool(t)
m.cut(r0, milling.POCKET | milling.CONVENTIONAL, 0)
m.cut(r1, milling.POCKET | milling.CONVENTIONAL, depth)
m.cut(r2, milling.POCKET | milling.CONVENTIONAL, depth)
m.cut(r3, milling.POCKET | milling.CONVENTIONAL, depth)
m.stop()

print "\n".join(m.output())

