#!/usr/bin/python3

import math

class XYZ(object):
    def __init__(self, x = 0, y = 0, z = 0):
        self.x = x
        self.y = y
        self.z = z

    def translate(self, dx = 0, dy = 0, dz = 0):
        self.x = self.x + dx
        self.y = self.y + dy
        self.z = self.z + dz

class Props(object):
    def __init__(self, piece_width = 10.0, piece_height = 0.995, piece_depth = 0.995,
            cam_width = 1.15, hole_width = 0.78, cam_angle_deg = 20, bit_radius = 0.125,
            port_height = 0.80, cam_centers = 6.2, plunge_feed = 4, cut_feed = 6,
            wall_thickness = 0.0625, cut_through = 0.0375):
        self.piece_width = piece_width
        self.piece_height = piece_height
        self.piece_depth = piece_depth
        self.cam_width = cam_width
        self.hole_width = hole_width
        self.cam_angle_deg = cam_angle_deg
        self.bit_radius = bit_radius
        self.port_height = port_height
        self.cam_centers = cam_centers
        self.plunge_feed = plunge_feed
        self.cut_feed = cut_feed
        self.wall_thickness = wall_thickness
        self.cut_through = cut_through
        self.cos = math.cos(self.cam_angle_deg * math.pi / 180.0)
        self.sin = math.sin(self.cam_angle_deg * math.pi / 180.0)
        self.port_center_left = (self.piece_width - self.cam_centers) * 0.5
        self.port_center_right = self.piece_width - self.port_center_left
        self.port_mult_x = 1.0 / self.cos

    def co_center_of_hole(self, left):
        c = self.co_center_of_square(left, True)
        y = self.piece_height * 0.5
        x = c.x + self.dxdy(left) * y
        z = -self.piece_depth + self.wall_thickness
        return XYZ(x, y, z)

    def rad_hole(self):
        return self.hole_width * 0.5 - self.bit_radius

    def co_start_of_hole(self, left):
        c = self.co_center_of_hole(left)
        r = self.rad_hole()
        x = c.x - r
        c.z -= self.wall_thickness + self.cut_through
        return XYZ(x, c.y, c.z)

    def hole_arc(self, left):
        c = self.co_start_of_hole(left)
        r = self.rad_hole()
        return "G17\n" + "G02X%.4fI%.4fF%g" % (c.x, r, self.cut_feed)

    def co_center_of_port(self, left, near):
        c = self.co_center_of_square(left, near)
        c.z = -self.port_height + self.cam_width * 0.5
        return c

    def rad_port(self):
        return self.cam_width * 0.5 - self.bit_radius

    def port_arc(self, left, near):
        c = self.co_center_of_port(left, near)
        r = self.rad_port()
        x = c.x - r
        return "G01X%.4fZ%.4fF%g\n" % (x, c.z - self.bit_radius, self.cut_feed) + \
            "G18\n" + "G02X%.4fI%.4fF%g" % (x + 2 * r, r, self.plunge_feed)

    def port_start(self, left, near, adjust):
        c = self.co_center_of_square(left, near)
        if adjust:
            if near:
                c.y += self.wall_thickness
                c.x += self.dxdy(left) * self.wall_thickness
            else:
                c.y -= self.wall_thickness
                c.x -= self.dxdy(left) * self.wall_thickness
        c.x -= self.rad_port()
        c.z = self.port_bottom() + self.rad_port()
        return c

    def port_bottom(self):
        return -self.port_height

    def port_width_range(self, z, left, near):
        c = self.co_center_of_port(left, near)
        cr = self.cam_width * 0.5
        if (z > c.z):
            return (c.x - cr, c.x + cr)
        if (z < self.port_bottom()):
            return (c.x, c.x)
        w = math.sqrt(cr * cr - (c.z - z) * (c.z - z))
        return (c.x - w, c.x + w)

    def port_depth(self, x, left, near):
        c = self.co_center_of_port(left, near)
        cr = self.cam_width * 0.5
        dx = x - c.x
        if dx < -cr or dx > cr:
            return 0
        return c.z - math.sqrt(cr * cr - dx * dx)

    def port_rough(self, left, near):
        c1 = self.port_start(left, near, False)
        c2 = self.port_start(left, near, True)
        c = XYZ((c1.x + c2.x) * 0.5, (c1.y + c2.y) * 0.5, (c1.z + c2.z) * 0.5)
        l = []
        l.append(self.z_move_to(XYZ(c.x, c.y, 0)))
        l.append("G01Z%.4fF%g" % (c.z, self.plunge_feed))
        l.append("G01X%.4fF%g" % (c.x + 2 * self.rad_port(), self.cut_feed))
        dz = 0.25
        br = self.bit_radius
        while dz < self.cam_width * 0.5:
            xx = self.port_width_range(c.z - dz, left, near)
            if (xx[1] - xx[0] > br * 2):
                l.append("G00X%.4fF20" % (xx[0] + br,))
                l.append("G01Z%.4fF%g" % (c.z - dz, self.plunge_feed))
                l.append("G01X%.4fF%g" % (xx[1] - br, self.cut_feed))
            if dz + 0.35 < self.cam_width * 0.5:
                dz = dz + 0.25
            elif dz + 0.20 < self.cam_width * 0.5:
                dz = dz + 0.1
            else:
                dz = dz + 0.05
        return "\n".join(l)

    def co_center_of_square(self, left, near):
        y = 0
        if left:
            x = self.port_center_left
        else:
            x = self.port_center_right
        if not near:
            y = self.piece_height
            x = x + self.dxdy(left) * y
        return XYZ(x, y, 0)

    def co_square_half_width(self):
        return (self.cam_width * 0.5 - self.bit_radius) * self.port_mult_x

    def top_square(self, left):
        c = self.co_center_of_square(left, True)
        y1 = -self.bit_radius
        x1 = c.x - self.co_square_half_width() + self.dxdy(left) * y1
        y2 = self.piece_height + self.bit_radius
        x2 = x1 + self.dxdy(left) * (y2 - y1)
        y21 = self.piece_height - self.wall_thickness * 0.5
        x21 = c.x - self.co_square_half_width() + self.dxdy(left) * y21
        x22 = x21 + 2 * self.co_square_half_width()
        x3 = x2 + 2 * self.co_square_half_width()
        x4 = x1 + 2 * self.co_square_half_width()
        y41 = self.wall_thickness * 0.5
        x41 = c.x + self.co_square_half_width() + self.dxdy(left) * y41
        x42 = c.x - self.co_square_half_width() + self.dxdy(left) * y41
        return self.z_move_to(XYZ(x1, y1, 0.1)) + "\n" + \
            self.cut_down(True) + "\n" + \
            "G01X%.4fY%.4fF%g\n" % (x2, y2, self.cut_feed) + \
            "G01X%.4fY%.4fF%g\n" % (x21, y21, self.cut_feed) + \
            "G01X%.4fY%.4fF%g\n" % (x22, y21, self.cut_feed) + \
            "G01X%.4fY%.4fF%g\n" % (x3, y2, self.cut_feed) + \
            "G01X%.4fY%.4fF%g\n" % (x4, y1, self.cut_feed) + \
            "G01X%.4fY%.4fF%g\n" % (x41, y41, self.cut_feed) + \
            "G01X%.4fY%.4fF%g\n" % (x42, y41, self.cut_feed) + \
            "G00Z0.1F20"

    def z_move_to(self, co):
        # use cut feed for downwards motions, in case there's some interference
        return "G00Z0.1F20\n" + \
            "G00X%.4fY%.4fF20\n" % (co.x, co.y) + \
            "G01Z%.4fF%g" % (co.z, self.cut_feed)

    def cut_down(self, top):
        if top:
            return "G01Z%.4fF%g" % (-self.wall_thickness-self.cut_through, self.plunge_feed)
        else:
            return "G01Z%.4fF%g" % (-self.piece_depth-self.cut_through, self.plunge_feed)

    def dxdy(self, left):
        if left:
            return -self.sin / self.cos
        return self.sin / self.cos

    def cut_to(self, dst):
        return "G01X%.4fY%.4fZ%.4fF%g" % (dst.x, dst.y, dst.z, self.cut_feed)

def print_header(props):
    print("%")
    print("M03")
    print("G97S3000")
    print("G20")
    print("G17")
    print("G91.1")
    print("G94")
    print("")

def print_trailer(props):
    print("")
    print("G00Z1")
    print("M5")
    print("G00X0Y0")
    print("M30")
    print("")

def print_top_bottom(props):
    print("(top square left)")
    print(props.top_square(True))
    print("")
    print("(top square right)")
    print(props.top_square(False))
    print("")
    print("(hole left)")
    print(props.z_move_to(props.co_center_of_hole(True)))
    print(props.cut_down(False))
    print(props.cut_to(props.co_start_of_hole(True)))
    print(props.hole_arc(True))
    print("")
    print("(hole right)")
    print(props.z_move_to(props.co_center_of_hole(False)))
    print(props.cut_down(False))
    print(props.cut_to(props.co_start_of_hole(False)))
    print(props.hole_arc(False))
    print("")

def print_rough_port(props):
    print("(left near port mill)")
    print(props.port_rough(True, True))
    print("")
    print("(left far port mill)")
    print(props.port_rough(True, False))
    print("")
    print("(right near port mill)")
    print(props.port_rough(False, True))
    print("")
    print("(right far port mill)")
    print(props.port_rough(False, False))
    print("")

def print_ball(props):
    print("(left near port ball finish)")
    print(props.z_move_to(props.port_start(True, True, False)))
    print(props.port_arc(True, True))
    print(props.z_move_to(props.port_start(True, True, True)))
    print(props.port_arc(True, True))
    print("")
    print("(left far port ball finish)")
    print(props.z_move_to(props.port_start(True, False, False)))
    print(props.port_arc(True, False))
    print(props.z_move_to(props.port_start(True, False, True)))
    print(props.port_arc(True, False))
    print("")
    print("(right near port ball finish)")
    print(props.z_move_to(props.port_start(False, True, False)))
    print(props.port_arc(False, True))
    print(props.z_move_to(props.port_start(False, True, True)))
    print(props.port_arc(False, True))
    print("")
    print("(right far port ball finish)")
    print(props.z_move_to(props.port_start(False, False, False)))
    print(props.port_arc(False, False))
    print(props.z_move_to(props.port_start(False, False, True)))
    print(props.port_arc(False, False))
    print("")

props = Props()

is_top_bottom = True
is_rough_port = False
is_ball = False;

print_header(props)

if is_top_bottom:
    print_top_bottom(props)
if is_rough_port:
    print_rough_port(props)
if is_ball:
    print_ball(props)

print_trailer(props)

