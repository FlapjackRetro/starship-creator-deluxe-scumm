/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/algorithm.h"
#include "common/debug.h"
#include "common/util.h"
#include "common/hashmap.h"
#include <math.h>

#include "graphics/macgui/macwidget.h"
#include "graphics/macgui/macwindowmanager.h"

#include "director/director.h"
#include "director/cast.h"
#include "director/castmember/xtra.h"
#include "director/channel.h"
#include "director/lingo/lingo.h"
#include "director/lingo/lingo-the.h"
#include "director/lingo/xtras-cast/flashxtra.h"
#include "director/window.h"

namespace Director {

namespace FlashXtra {

CastMember *createCastMember(Cast *cast, uint16 castId, XtraCastMember *xtra) {
	return new FlashXtraCastMember(cast, castId, *xtra);
}

// ---------------------------------------------------------------------------
// Matrix
// ---------------------------------------------------------------------------

SWFMovie::Matrix SWFMovie::Matrix::operator*(const Matrix &o) const {
	Matrix r;
	r.a = a * o.a + c * o.b;
	r.b = b * o.a + d * o.b;
	r.c = a * o.c + c * o.d;
	r.d = b * o.c + d * o.d;
	r.tx = a * o.tx + c * o.ty + tx;
	r.ty = b * o.tx + d * o.ty + ty;
	return r;
}

SWFMovie::Matrix SWFMovie::Matrix::inverse() const {
	Matrix r;
	float det = a * d - b * c;
	if (fabsf(det) < 1e-12f)
		return r;
	r.a = d / det;
	r.b = -b / det;
	r.c = -c / det;
	r.d = a / det;
	r.tx = -(r.a * tx + r.c * ty);
	r.ty = -(r.b * tx + r.d * ty);
	return r;
}

// ---------------------------------------------------------------------------
// Bit reader
// ---------------------------------------------------------------------------

namespace {

struct Reader {
	const byte *data;
	uint32 size;
	uint32 pos;
	uint32 bitPos;	// 0 = byte aligned
	bool eof;

	Reader(const byte *d, uint32 s) : data(d), size(s), pos(0), bitPos(0), eof(false) {}

	void align() {
		if (bitPos) {
			bitPos = 0;
			pos++;
		}
	}
	byte u8() {
		align();
		if (pos >= size) {
			eof = true;
			return 0;
		}
		return data[pos++];
	}
	uint16 u16() {
		uint16 lo = u8();
		uint16 hi = u8();
		return lo | (hi << 8);
	}
	int16 s16() { return (int16)u16(); }
	uint32 u32() {
		uint32 lo = u16();
		uint32 hi = u16();
		return lo | (hi << 16);
	}
	uint32 ub(int n) {
		uint32 v = 0;
		for (int i = 0; i < n; i++) {
			if (pos >= size) {
				eof = true;
				return v << (n - i);
			}
			int bit = (data[pos] >> (7 - bitPos)) & 1;
			v = (v << 1) | bit;
			if (++bitPos == 8) {
				bitPos = 0;
				pos++;
			}
		}
		return v;
	}
	int32 sb(int n) {
		if (n == 0)
			return 0;
		uint32 v = ub(n);
		if (v & (1u << (n - 1)))
			return (int32)(v | ~((1u << n) - 1));
		return (int32)v;
	}
	float fb(int n) { return sb(n) / 65536.0f; }
	bool atEnd() const { return eof || pos >= size; }
};

SWFMovie::Matrix readMatrix(Reader &r) {
	SWFMovie::Matrix m;
	r.align();
	if (r.ub(1)) {
		int n = r.ub(5);
		m.a = r.fb(n);
		m.d = r.fb(n);
	}
	if (r.ub(1)) {
		int n = r.ub(5);
		m.b = r.fb(n);
		m.c = r.fb(n);
	}
	int n = r.ub(5);
	m.tx = (float)r.sb(n);
	m.ty = (float)r.sb(n);
	r.align();
	return m;
}

void readRect(Reader &r, int &xmin, int &xmax, int &ymin, int &ymax) {
	r.align();
	int n = r.ub(5);
	xmin = r.sb(n);
	xmax = r.sb(n);
	ymin = r.sb(n);
	ymax = r.sb(n);
	r.align();
}

SWFMovie::ColorXform readCxform(Reader &r) {
	SWFMovie::ColorXform c;
	r.align();
	bool hasAdd = r.ub(1);
	bool hasMult = r.ub(1);
	int n = r.ub(4);
	if (hasMult) {
		c.mr = r.sb(n) / 256.0f;
		c.mg = r.sb(n) / 256.0f;
		c.mb = r.sb(n) / 256.0f;
	}
	if (hasAdd) {
		c.ar = (float)r.sb(n);
		c.ag = (float)r.sb(n);
		c.ab = (float)r.sb(n);
	}
	r.align();
	return c;
}

void readFillStyles(Reader &r, Common::Array<SWFMovie::FillStyle> &fills, bool shape2) {
	int count = r.u8();
	if (count == 0xFF && shape2)
		count = r.u16();
	for (int i = 0; i < count && !r.atEnd(); i++) {
		SWFMovie::FillStyle fs;
		int type = r.u8();
		if (type == 0x00) {
			fs.type = 0;
			fs.r = r.u8();
			fs.g = r.u8();
			fs.b = r.u8();
		} else if (type == 0x10 || type == 0x12) {
			fs.type = (type == 0x10) ? 1 : 2;
			fs.matrix = readMatrix(r);
			int n = r.u8();
			for (int j = 0; j < n; j++) {
				fs.ratios.push_back(r.u8());
				fs.colors.push_back(r.u8());
				fs.colors.push_back(r.u8());
				fs.colors.push_back(r.u8());
			}
		} else if (type >= 0x40 && type <= 0x43) {
			fs.type = 3;
			r.u16();	// bitmap id
			fs.matrix = readMatrix(r);
			fs.r = fs.g = fs.b = 128;
		} else {
			r.eof = true;
		}
		fills.push_back(fs);
	}
}

void readLineStyles(Reader &r, Common::Array<SWFMovie::LineStyle> &lines, bool shape2) {
	int count = r.u8();
	if (count == 0xFF && shape2)
		count = r.u16();
	for (int i = 0; i < count && !r.atEnd(); i++) {
		SWFMovie::LineStyle ls;
		ls.width = r.u16();
		ls.r = r.u8();
		ls.g = r.u8();
		ls.b = r.u8();
		lines.push_back(ls);
	}
}

// Reads shape records (after the style arrays when withStyles is true).
void readShapeRecords(Reader &r, SWFMovie::Shape &shape, bool withStyles, bool shape2, int implicitFill) {
	int fillBits = r.ub(4);
	int lineBits = r.ub(4);
	float x = 0, y = 0;
	int fill0 = implicitFill, fill1 = implicitFill, line = 0;
	int fillBase = 0, lineBase = 0;
	(void)withStyles;

	while (!r.atEnd()) {
		if (r.ub(1) == 0) {
			int flags = r.ub(5);
			if (flags == 0)
				break;
			bool newStyles = flags & 16;
			bool lineStyle = flags & 8;
			bool fillStyle1 = flags & 4;
			bool fillStyle0 = flags & 2;
			bool moveTo = flags & 1;
			if (moveTo) {
				int n = r.ub(5);
				x = (float)r.sb(n);
				y = (float)r.sb(n);
			}
			if (fillStyle0) {
				int v = r.ub(fillBits);
				fill0 = v ? v + fillBase : 0;
			}
			if (fillStyle1) {
				int v = r.ub(fillBits);
				fill1 = v ? v + fillBase : 0;
			}
			if (lineStyle) {
				int v = r.ub(lineBits);
				line = v ? v + lineBase : 0;
			}
			if (newStyles && shape2) {
				fillBase = shape.fills.size();
				lineBase = shape.lines.size();
				readFillStyles(r, shape.fills, true);
				readLineStyles(r, shape.lines, true);
				fillBits = r.ub(4);
				lineBits = r.ub(4);
			}
		} else {
			SWFMovie::Edge e;
			e.fill0 = fill0;
			e.fill1 = fill1;
			e.line = line;
			e.x0 = x;
			e.y0 = y;
			if (r.ub(1)) {	// straight
				int n = r.ub(4) + 2;
				float dx = 0, dy = 0;
				if (r.ub(1)) {
					dx = (float)r.sb(n);
					dy = (float)r.sb(n);
				} else if (r.ub(1)) {
					dy = (float)r.sb(n);
				} else {
					dx = (float)r.sb(n);
				}
				x += dx;
				y += dy;
				e.curve = false;
				e.cx = e.cy = 0;
			} else {
				int n = r.ub(4) + 2;
				float cdx = (float)r.sb(n);
				float cdy = (float)r.sb(n);
				float adx = (float)r.sb(n);
				float ady = (float)r.sb(n);
				e.cx = x + cdx;
				e.cy = y + cdy;
				x = e.cx + adx;
				y = e.cy + ady;
				e.curve = true;
			}
			e.x1 = x;
			e.y1 = y;
			shape.edges.push_back(e);
		}
	}
	r.align();
}

} // End of anonymous namespace

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

bool SWFMovie::load(const byte *data, uint32 size) {
	if (size < 12 || data[0] != 'F' || data[1] != 'W' || data[2] != 'S')
		return false;

	Reader r(data, size);
	r.pos = 8;
	readRect(r, _xmin, _xmax, _ymin, _ymax);
	r.u16();	// frame rate (8.8)
	_frameCount = r.u16();

	DisplayList current;

	while (!r.atEnd()) {
		uint16 hdr = r.u16();
		int tag = hdr >> 6;
		uint32 len = hdr & 63;
		if (len == 63)
			len = r.u32();
		if (r.pos + len > size)
			break;

		Reader t(data + r.pos, len);
		r.pos += len;

		if (tag == 0) {
			break;
		} else if (tag == 1) {		// ShowFrame
			_frames.push_back(current);
		} else if (tag == 2 || tag == 22) {	// DefineShape, DefineShape2
			bool shape2 = (tag == 22);
			int id = t.u16();
			int x0, x1, y0, y1;
			readRect(t, x0, x1, y0, y1);
			Shape s;
			readFillStyles(t, s.fills, shape2);
			readLineStyles(t, s.lines, shape2);
			readShapeRecords(t, s, true, shape2, 0);
			_shapes[id] = s;
		} else if (tag == 9) {		// SetBackgroundColor
			_bgR = t.u8();
			_bgG = t.u8();
			_bgB = t.u8();
		} else if (tag == 4) {		// PlaceObject
			Placement p;
			p.id = t.u16();
			p.depth = t.u16();
			p.matrix = readMatrix(t);
			if (!t.atEnd())
				p.cxform = readCxform(t);
			// replace anything already at this depth
			bool replaced = false;
			for (uint i = 0; i < current.size(); i++) {
				if (current[i].depth == p.depth) {
					current[i] = p;
					replaced = true;
					break;
				}
			}
			if (!replaced)
				current.push_back(p);
		} else if (tag == 5) {		// RemoveObject
			t.u16();
			int depth = t.u16();
			for (uint i = 0; i < current.size(); i++) {
				if (current[i].depth == depth) {
					current.remove_at(i);
					break;
				}
			}
		} else if (tag == 10) {		// DefineFont
			int id = t.u16();
			uint32 base = t.pos;
			int first = t.u16();
			int count = first / 2;
			Common::Array<int> offsets;
			offsets.push_back(first);
			for (int i = 1; i < count; i++)
				offsets.push_back(t.u16());
			Common::Array<Shape> glyphs;
			for (int i = 0; i < count; i++) {
				Reader g(t.data, t.size);
				g.pos = base + offsets[i];
				Shape gs;
				if (g.pos < t.size)
					readShapeRecords(g, gs, false, false, 1);
				glyphs.push_back(gs);
			}
			_fontGlyphs[id] = glyphs;
		} else if (tag == 11) {		// DefineText
			int id = t.u16();
			int x0, x1, y0, y1;
			readRect(t, x0, x1, y0, y1);
			Text text;
			text.matrix = readMatrix(t);
			int glyphBits = t.u8();
			int advBits = t.u8();
			int font = 0;
			float height = 0, tx = 0, ty = 0;
			byte cr = 0, cg = 0, cb = 0;
			while (!t.atEnd()) {
				int b = t.u8();
				if (b == 0)
					break;
				if (b & 0x80) {
					bool hasFont = b & 8, hasColor = b & 4, hasY = b & 2, hasX = b & 1;
					if (hasFont)
						font = t.u16();
					if (hasColor) {
						cr = t.u8();
						cg = t.u8();
						cb = t.u8();
					}
					if (hasX)
						tx = (float)t.s16();
					if (hasY)
						ty = (float)t.s16();
					if (hasFont)
						height = (float)t.u16();
				} else {
					int n = b & 0x7f;
					for (int i = 0; i < n; i++) {
						TextGlyph g;
						g.glyph = t.ub(glyphBits);
						int adv = t.sb(advBits);
						g.font = font;
						g.x = tx;
						g.y = ty;
						g.height = height;
						g.r = cr;
						g.g = cg;
						g.b = cb;
						text.glyphs.push_back(g);
						tx += adv;
					}
					t.align();
				}
			}
			_texts[id] = text;
		}
		// everything else (bitmaps, buttons, sounds, actions) is ignored
	}

	return !_frames.empty() || !_shapes.empty();
}

// ---------------------------------------------------------------------------
// Rasterizer
// ---------------------------------------------------------------------------

class Rasterizer {
public:
	struct Seg {
		float x0, y0, x1, y1;
	};

	Rasterizer(int w, int h) : _w(w), _h(h) {}

	// Fill the polygon set (non-zero winding) with a solid colour or a gradient.
	void fill(const Common::Array<Seg> &segs, const SWFMovie::FillStyle &fs, const SWFMovie::Matrix &toDevice,
			const SWFMovie::ColorXform &cx, Common::Array<byte> &rgba);

	static void flatten(const SWFMovie::Edge &e, const SWFMovie::Matrix &m, Common::Array<Seg> &out, bool reverse);

private:
	int _w, _h;
};

void Rasterizer::flatten(const SWFMovie::Edge &e, const SWFMovie::Matrix &m, Common::Array<Seg> &out, bool reverse) {
	float px, py, qx, qy;
	m.apply(e.x0, e.y0, px, py);
	m.apply(e.x1, e.y1, qx, qy);

	Common::Array<Seg> tmp;
	if (!e.curve) {
		Seg s = { px, py, qx, qy };
		tmp.push_back(s);
	} else {
		float cx, cy;
		m.apply(e.cx, e.cy, cx, cy);
		float dev = fabsf(px - 2 * cx + qx) + fabsf(py - 2 * cy + qy);
		int steps = CLIP((int)(sqrtf(dev) * 1.5f) + 1, 2, 24);
		float lx = px, ly = py;
		for (int i = 1; i <= steps; i++) {
			float t = (float)i / steps;
			float u = 1 - t;
			float x = u * u * px + 2 * u * t * cx + t * t * qx;
			float y = u * u * py + 2 * u * t * cy + t * t * qy;
			Seg s = { lx, ly, x, y };
			tmp.push_back(s);
			lx = x;
			ly = y;
		}
	}

	if (reverse) {
		for (int i = (int)tmp.size() - 1; i >= 0; i--) {
			Seg s = { tmp[i].x1, tmp[i].y1, tmp[i].x0, tmp[i].y0 };
			out.push_back(s);
		}
	} else {
		for (uint i = 0; i < tmp.size(); i++)
			out.push_back(tmp[i]);
	}
}

static void gradientColor(const SWFMovie::FillStyle &fs, float t, float &r, float &g, float &b) {
	t = CLIP(t, 0.0f, 1.0f);
	int n = fs.ratios.size();
	if (n == 0) {
		r = g = b = 0;
		return;
	}
	float pos = t * 255.0f;
	if (pos <= fs.ratios[0] || n == 1) {
		r = fs.colors[0]; g = fs.colors[1]; b = fs.colors[2];
		return;
	}
	for (int i = 1; i < n; i++) {
		if (pos <= fs.ratios[i]) {
			float span = (float)(fs.ratios[i] - fs.ratios[i - 1]);
			float f = span > 0 ? (pos - fs.ratios[i - 1]) / span : 1.0f;
			r = fs.colors[(i - 1) * 3 + 0] * (1 - f) + fs.colors[i * 3 + 0] * f;
			g = fs.colors[(i - 1) * 3 + 1] * (1 - f) + fs.colors[i * 3 + 1] * f;
			b = fs.colors[(i - 1) * 3 + 2] * (1 - f) + fs.colors[i * 3 + 2] * f;
			return;
		}
	}
	r = fs.colors[(n - 1) * 3 + 0]; g = fs.colors[(n - 1) * 3 + 1]; b = fs.colors[(n - 1) * 3 + 2];
}

void Rasterizer::fill(const Common::Array<Seg> &segs, const SWFMovie::FillStyle &fs, const SWFMovie::Matrix &toDevice,
		const SWFMovie::ColorXform &cx, Common::Array<byte> &rgba) {
	if (segs.empty())
		return;

	// bounds
	float minY = 1e9f, maxY = -1e9f;
	for (uint i = 0; i < segs.size(); i++) {
		minY = MIN(minY, MIN(segs[i].y0, segs[i].y1));
		maxY = MAX(maxY, MAX(segs[i].y0, segs[i].y1));
	}
	int y0 = MAX(0, (int)floorf(minY));
	int y1 = MIN(_h - 1, (int)floorf(maxY));

	// gradient setup: gradient space is a 32768-twip square centred on the origin
	SWFMovie::Matrix inv;
	if (fs.type == 1 || fs.type == 2)
		inv = (toDevice * fs.matrix).inverse();

	const int SUB = 4;
	Common::Array<float> cover(_w);

	struct Crossing {
		float x;
		int dir;
	};
	Common::Array<Crossing> xs;

	for (int y = y0; y <= y1; y++) {
		for (int i = 0; i < _w; i++)
			cover[i] = 0;
		bool any = false;

		for (int s = 0; s < SUB; s++) {
			float sy = y + (s + 0.5f) / SUB;
			xs.clear();
			for (uint i = 0; i < segs.size(); i++) {
				const Seg &sg = segs[i];
				float ya = sg.y0, yb = sg.y1;
				if (ya == yb)
					continue;
				int dir = 1;
				float xa = sg.x0, xb = sg.x1;
				if (ya > yb) {
					SWAP(ya, yb);
					SWAP(xa, xb);
					dir = -1;
				}
				if (sy < ya || sy >= yb)
					continue;
				float t = (sy - ya) / (yb - ya);
				Crossing c = { xa + t * (xb - xa), dir };
				xs.push_back(c);
			}
			if (xs.size() < 2)
				continue;
			Common::sort(xs.begin(), xs.end(), [](const Crossing &a, const Crossing &b) { return a.x < b.x; });

			int wind = 0;
			for (uint i = 0; i + 1 < xs.size(); i++) {
				wind += xs[i].dir;
				if (wind == 0)
					continue;
				float xa = xs[i].x, xb = xs[i + 1].x;
				if (xb <= 0 || xa >= _w)
					continue;
				xa = MAX(xa, 0.0f);
				xb = MIN(xb, (float)_w);
				int ia = (int)floorf(xa);
				int ib = (int)floorf(xb);
				if (ia == ib) {
					cover[ia] += (xb - xa) / SUB;
				} else {
					cover[ia] += (ia + 1 - xa) / SUB;
					for (int x = ia + 1; x < ib; x++)
						cover[x] += 1.0f / SUB;
					if (ib < _w)
						cover[ib] += (xb - ib) / SUB;
				}
				any = true;
			}
		}
		if (!any)
			continue;

		for (int x = 0; x < _w; x++) {
			float c = cover[x];
			if (c <= 0.001f)
				continue;
			if (c > 1.0f)
				c = 1.0f;

			float r, g, b;
			if (fs.type == 1 || fs.type == 2) {
				float gx, gy;
				inv.apply(x + 0.5f, y + 0.5f, gx, gy);
				float t;
				if (fs.type == 1)
					t = (gx + 16384.0f) / 32768.0f;
				else
					t = sqrtf(gx * gx + gy * gy) / 16384.0f;
				gradientColor(fs, t, r, g, b);
			} else {
				r = fs.r;
				g = fs.g;
				b = fs.b;
			}
			r = CLIP(r * cx.mr + cx.ar, 0.0f, 255.0f);
			g = CLIP(g * cx.mg + cx.ag, 0.0f, 255.0f);
			b = CLIP(b * cx.mb + cx.ab, 0.0f, 255.0f);

			byte *px = &rgba[(y * _w + x) * 4];
			px[0] = (byte)(px[0] + (r - px[0]) * c + 0.5f);
			px[1] = (byte)(px[1] + (g - px[1]) * c + 0.5f);
			px[2] = (byte)(px[2] + (b - px[2]) * c + 0.5f);
			px[3] = 255;
		}
	}
}

// Draw one shape instance
static void drawShape(const SWFMovie::Shape &shape, const SWFMovie::Matrix &m, const SWFMovie::ColorXform &cx,
		Rasterizer &rz, Common::Array<byte> &rgba, float scale, int implicitFillColor,
		const SWFMovie::FillStyle *override) {
	// fills
	for (uint f = 0; f < shape.fills.size() || (override && f == 0); f++) {
		Common::Array<Rasterizer::Seg> segs;
		int idx = f + 1;
		for (uint i = 0; i < shape.edges.size(); i++) {
			const SWFMovie::Edge &e = shape.edges[i];
			if (e.fill1 == idx)
				Rasterizer::flatten(e, m, segs, false);
			if (e.fill0 == idx)
				Rasterizer::flatten(e, m, segs, true);
		}
		if (segs.empty())
			continue;
		const SWFMovie::FillStyle &fs = override ? *override : shape.fills[f];
		rz.fill(segs, fs, m, cx, rgba);
		if (override)
			break;
	}
	(void)implicitFillColor;

	// strokes
	for (uint l = 0; l < shape.lines.size(); l++) {
		int idx = l + 1;
		Common::Array<Rasterizer::Seg> quads;	// emitted as 4 segments per quad
		float hw = MAX(shape.lines[l].width * scale, 1.0f) * 0.5f;
		for (uint i = 0; i < shape.edges.size(); i++) {
			const SWFMovie::Edge &e = shape.edges[i];
			if (e.line != idx)
				continue;
			Common::Array<Rasterizer::Seg> pieces;
			Rasterizer::flatten(e, m, pieces, false);
			for (uint k = 0; k < pieces.size(); k++) {
				const Rasterizer::Seg &s = pieces[k];
				float dx = s.x1 - s.x0, dy = s.y1 - s.y0;
				float len = sqrtf(dx * dx + dy * dy);
				if (len < 1e-4f)
					continue;
				float ux = dx / len, uy = dy / len;
				float nx = -uy * hw, ny = ux * hw;
				float ex = ux * hw * 0.5f, ey = uy * hw * 0.5f;
				float ax = s.x0 - ex, ay = s.y0 - ey;
				float bx = s.x1 + ex, by = s.y1 + ey;
				// consistent orientation: (a+n) -> (b+n) -> (b-n) -> (a-n)
				Rasterizer::Seg q0 = { ax + nx, ay + ny, bx + nx, by + ny };
				Rasterizer::Seg q1 = { bx + nx, by + ny, bx - nx, by - ny };
				Rasterizer::Seg q2 = { bx - nx, by - ny, ax - nx, ay - ny };
				Rasterizer::Seg q3 = { ax - nx, ay - ny, ax + nx, ay + ny };
				quads.push_back(q0);
				quads.push_back(q1);
				quads.push_back(q2);
				quads.push_back(q3);
			}
		}
		if (quads.empty())
			continue;
		SWFMovie::FillStyle ls;
		ls.type = 0;
		ls.r = shape.lines[l].r;
		ls.g = shape.lines[l].g;
		ls.b = shape.lines[l].b;
		// Each quad has the same orientation, so non-zero winding gives their union.
		rz.fill(quads, ls, m, cx, rgba);
	}
}

void SWFMovie::render(int frame, int width, int height, Common::Array<byte> &rgba, const Matrix *stageOverride) const {
	rgba.resize(width * height * 4);
	for (int i = 0; i < width * height; i++) {
		rgba[i * 4 + 0] = _bgR;
		rgba[i * 4 + 1] = _bgG;
		rgba[i * 4 + 2] = _bgB;
		rgba[i * 4 + 3] = 255;
	}
	if (_frames.empty() || width <= 0 || height <= 0)
		return;

	frame = CLIP(frame, 0, (int)_frames.size() - 1);
	const DisplayList &dl = _frames[frame];

	float sw = (float)(_xmax - _xmin);
	float sh = (float)(_ymax - _ymin);
	if (sw <= 0 || sh <= 0)
		return;

	Matrix stage;
	stage.a = width / sw;
	stage.d = height / sh;
	stage.tx = -_xmin * stage.a;
	stage.ty = -_ymin * stage.d;
	if (stageOverride)
		stage = *stageOverride;
	float scale = (stage.a + stage.d) * 0.5f;

	// depth order
	Common::Array<Placement> sorted = dl;
	Common::sort(sorted.begin(), sorted.end(), [](const Placement &a, const Placement &b) { return a.depth < b.depth; });

	Rasterizer rz(width, height);

	for (uint i = 0; i < sorted.size(); i++) {
		const Placement &p = sorted[i];
		Matrix m = stage * p.matrix;

		if (_shapes.contains(p.id)) {
			drawShape(_shapes.getVal(p.id), m, p.cxform, rz, rgba, scale, 0, nullptr);
		} else if (_texts.contains(p.id)) {
			const Text &t = _texts.getVal(p.id);
			Matrix tm = m * t.matrix;
			for (uint g = 0; g < t.glyphs.size(); g++) {
				const TextGlyph &tg = t.glyphs[g];
				if (!_fontGlyphs.contains(tg.font))
					continue;
				const Common::Array<Shape> &glyphs = _fontGlyphs.getVal(tg.font);
				if (tg.glyph < 0 || tg.glyph >= (int)glyphs.size())
					continue;
				Matrix gm;
				float gs = tg.height / 1024.0f;
				gm.a = gs;
				gm.d = gs;
				gm.tx = tg.x;
				gm.ty = tg.y;
				FillStyle fs;
				fs.type = 0;
				fs.r = tg.r;
				fs.g = tg.g;
				fs.b = tg.b;
				drawShape(glyphs[tg.glyph], tm * gm, p.cxform, rz, rgba, scale, 0, &fs);
			}
		}
	}
}

} // End of namespace FlashXtra

// ---------------------------------------------------------------------------
// Cast member
// ---------------------------------------------------------------------------

FlashXtraCastMember::FlashXtraCastMember(Cast *cast, uint16 castId, XtraCastMember &source)
		: CastMember(cast, castId) {
	_type = kCastXtra;
	_valid = false;
	_frame = 0;
	_viewSet = false;
	_viewH = _viewV = 0;
	_viewScale = 100;
	_originCenter = true;
	_regX = _regY = 0;
	const Common::Array<byte> &xd = source.getXtraData();
	if (xd.size() >= 144) {
		auto rdf = [&](int idx) {
			uint32 v = (xd[idx * 4] << 24) | (xd[idx * 4 + 1] << 16) | (xd[idx * 4 + 2] << 8) | xd[idx * 4 + 3];
			float f;
			memcpy(&f, &v, 4);
			return f;
		};
		_regX = rdf(34);
		_regY = rdf(35);
	}
}

FlashXtraCastMember::FlashXtraCastMember(Cast *cast, uint16 castId, FlashXtraCastMember &source)
		: CastMember(cast, castId) {
	_type = kCastXtra;
	_initialRect = source._initialRect;
	_boundingRect = source._boundingRect;
	_movie = source._movie;
	_valid = source._valid;
	_frame = source._frame;
	_viewSet = source._viewSet;
	_viewH = source._viewH;
	_viewV = source._viewV;
	_viewScale = source._viewScale;
	_originCenter = source._originCenter;
	_regX = source._regX;
	_regY = source._regY;
	_loaded = source._loaded;
	if (cast == source._cast)
		_children = source._children;
}

void FlashXtraCastMember::load() {
	if (_loaded)
		return;
	_loaded = true;

	for (auto &it : _children) {
		if (it.tag != MKTAG('X', 'M', 'E', 'D'))
			continue;
		if (!_cast->getArchive()->hasResource(it.tag, it.index)) {
			warning("FlashXtraCastMember::load(): XMED %d not found", it.index);
			break;
		}
		Common::SeekableReadStreamEndian *r = _cast->getArchive()->getResource(it.tag, it.index);
		Common::Array<byte> data(r->size());
		r->read(data.data(), r->size());
		delete r;

		// find the SWF signature inside the chunk
		for (uint i = 0; i + 12 < data.size(); i++) {
			if (data[i] == 'F' && data[i + 1] == 'W' && data[i + 2] == 'S') {
				_valid = _movie.load(&data[i], data.size() - i);
				break;
			}
		}
		if (_valid) {
			Common::Rect stage = _movie.getStageRect();
			if (_initialRect.isEmpty())
				_initialRect = stage;
			debugC(3, kDebugImages, "FlashXtraCastMember::load(): cast %d: %dx%d, %d frames", _castId, stage.width(), stage.height(), _movie.getFrameCount());
		} else {
			warning("FlashXtraCastMember::load(): cast %d: could not decode SWF", _castId);
		}
		break;
	}
}

Graphics::MacWidget *FlashXtraCastMember::createWidget(Common::Rect &bbox, Channel *channel, SpriteType spriteType) {
	if (!bbox.width() || !bbox.height())
		return nullptr;

	load();

	Graphics::MacWidget *widget = new Graphics::MacWidget(g_director->getCurrentWindow()->getMacWindow(),
			bbox.left, bbox.top, bbox.width(), bbox.height(), g_director->_wm, false);
	Graphics::ManagedSurface *surf = widget->getSurface();

	if (!_valid) {
		surf->fillRect(Common::Rect(surf->w, surf->h), g_director->_wm->_colorWhite);
		return widget;
	}

	Common::Array<byte> rgba;
	if (_viewSet) {
		FlashXtra::SWFMovie::Matrix m;
		float s = _viewScale / 100.0f;
		m.a = s / 20.0f;
		m.d = s / 20.0f;
		float ox = _originCenter ? bbox.width() / 2.0f : 0;
		float oy = _originCenter ? bbox.height() / 2.0f : 0;
		m.tx = ox - _viewH * s;
		m.ty = oy - _viewV * s;
		debugC(3, kDebugImages, "FlashXtraCastMember: cast %d bbox %dx%d view (%g,%g) scale %g center %d", _castId, bbox.width(), bbox.height(), _viewH, _viewV, _viewScale, _originCenter);
		_movie.render(_frame, bbox.width(), bbox.height(), rgba, &m);
	} else {
		debugC(3, kDebugImages, "FlashXtraCastMember: cast %d bbox %d,%d %dx%d fit frame %d ch %d vis %d", _castId, bbox.left, bbox.top, bbox.width(), bbox.height(), _frame, channel ? (int)channel->_sprite->_castId.member : -1, channel ? channel->_visible : -1);
		_movie.render(_frame, bbox.width(), bbox.height(), rgba);
	}

	const Graphics::PixelFormat &fmt = surf->format;
	// cache nearest-colour lookups for indexed output
	Common::HashMap<uint32, uint32> cache;
	for (int y = 0; y < bbox.height(); y++) {
		for (int x = 0; x < bbox.width(); x++) {
			const byte *p = &rgba[(y * bbox.width() + x) * 4];
			uint32 color;
			if (fmt.bytesPerPixel == 1) {
				uint32 key = (p[0] << 16) | (p[1] << 8) | p[2];
				if (cache.contains(key)) {
					color = cache.getVal(key);
				} else {
					color = g_director->_wm->findBestColor(p[0], p[1], p[2]);
					cache[key] = color;
				}
			} else {
				color = fmt.RGBToColor(p[0], p[1], p[2]);
			}
			if (fmt.bytesPerPixel == 1)
				*((byte *)surf->getBasePtr(x, y)) = (byte)color;
			else if (fmt.bytesPerPixel == 2)
				*((uint16 *)surf->getBasePtr(x, y)) = (uint16)color;
			else
				*((uint32 *)surf->getBasePtr(x, y)) = color;
		}
	}

	return widget;
}

bool FlashXtraCastMember::hasField(int field) {
	switch (field) {
	case kTheFrame:
	case kTheViewH:
	case kTheViewV:
	case kTheViewScale:
	case kTheViewPoint:
	case kTheOriginMode:
	case kTheStatic:
	case kTheImageEnabled:
		return true;
	default:
		break;
	}
	return CastMember::hasField(field);
}

Datum FlashXtraCastMember::getField(int field) {
	Datum d;

	switch (field) {
	case kTheCastType:
	case kTheType:
		d = Common::String("flash");
		d.type = SYMBOL;
		break;
	case kTheFrame:
		d = _frame + 1;
		break;
	case kTheViewH:
		d = (int)_viewH;
		break;
	case kTheViewV:
		d = (int)_viewV;
		break;
	case kTheViewScale:
		d = (int)_viewScale;
		break;
	case kTheOriginMode:
		d = Common::String(_originCenter ? "center" : "topLeft");
		d.type = SYMBOL;
		break;
	case kTheStatic:
	case kTheImageEnabled:
		d = 1;
		break;
	default:
		d = CastMember::getField(field);
		break;
	}

	return d;
}

void FlashXtraCastMember::setField(int field, const Datum &d) {
	switch (field) {
	case kTheFrame:
		_frame = MAX(0, d.asInt() - 1);
		setModified(true);
		return;
	case kTheViewH:
		_viewH = d.asFloat();
		_viewSet = true;
		setModified(true);
		return;
	case kTheViewV:
		_viewV = d.asFloat();
		_viewSet = true;
		setModified(true);
		return;
	case kTheViewScale:
		_viewScale = d.asFloat();
		_viewSet = true;
		setModified(true);
		return;
	case kTheViewPoint:
		if (d.type == POINT && d.u.farr->arr.size() >= 2) {
			_viewH = d.u.farr->arr[0].asFloat();
			_viewV = d.u.farr->arr[1].asFloat();
			_viewSet = true;
			setModified(true);
		}
		return;
	case kTheOriginMode:
		_originCenter = !d.asString().equalsIgnoreCase("topLeft");
		setModified(true);
		return;
	case kTheStatic:
	case kTheImageEnabled:
		return;
	default:
		break;
	}

	CastMember::setField(field, d);
}

Common::String FlashXtraCastMember::formatInfo() {
	return Common::String::format("initialRect: %dx%d, frames: %d", _initialRect.width(), _initialRect.height(), _movie.getFrameCount());
}

} // End of namespace Director
