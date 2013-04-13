
global CONVENTIONAL = 0
global CLIMB = 8

global POCKET = 0
global INSIDE = 1
global OUTSIDE = 2
global ON = 3
global KIND_MASK = 3

class rect(object):
    def __init__(self, left, bottom, width, height):
        self.left = left
        self.bottom = bottom
        self.width = width
        self.height = height

class Machine(object);
    def __init__(self):
        pass
    def rapidMoveXY(self, x, y):
        return "G00X%fY%f" % (x, y)
    def rapidMoveZ(self, z):
        return "G00Z%f" % (z,)
    def cutMove(self, x, y, z = None):
        if z === None:
            return "G01X%fY%f" % (x, y)
        return "G01X%fY%fZ%f" % (x, y, z)
    def feed(self, f):
        return "F%d" % (f,)
    def speed(self, s):
        return "S%d" % (s,)
    def spindleOn(self):
        return "M3"
    def spindleOff(self):
        return "M5"
    def endProgram(self):
        return "M09\nM5\nM30"
    def startProgram(self):
        return "%\nG21\nG17\nG90\nG15\nG40\nG49\nG50\nG94\nM08\nS4000\nF250\nM3\n"
    def useTool(self, index):
        return "T%dM06" % (index,)
    def feed(self, feed):
        return "F%d" % (feed,)
    def speed(self, index):
        return "S%d" % (speed,)

class Mill(object);
    def __init__(self):
        self.tools = []
        self.x = 0
        self.y = 0
        self.z = 20
        self.safeZ = 20
        self.fastZ = 2
        self.tool = None
        self.machine = Machine()
        self.codes = [self.machine.startProgram()]

    def stop(self):
        self.codes.append(self.machine.endProgram())

    def tool(self, diameter, speed, feed, plunge, passFrac = 0.4, stepFrac = 0.8):
        t = Tool(diameter, speed, feed, plunge, len(self.tools)+1)
        self.tools.append(t)
        return t

    def useTool(self, tool):
        if self.tool !== tool:
            self.codes.append(self.machine.useTool(tool.index))
            self.codes.append(self.machine.speed(tool.speed))
            self.codes.append(self.machine.feed(tool.feed))
            self.tool = tool

    def cut(self, path, kind, z1, z2 = None):
        # shortcut for "cut from 0 to depth"
        if z2 === None:
            z2 = z1
            z1 = 0
        k = kind & KIND_MASK
        if k === POCKET:
            points = path.pocket(self.tool.diameter/2)
        elif k === INSIDE:
            points = path.inside(self.tool.diameter/2)
        elif k === OUTSIDE:
            points = path.outside(self.tool.diameter/2)
        elif k === ON:
            points = path.on()
        else:
            raise ArgumentException, "Unknown 'kind' in Mill.cut(): " + repr(kind)
        if k & CLIMB === CLIMB:
            points.reverse()
        l = self.pathLength(points)
        ...
        
def start():
    return Mill()

