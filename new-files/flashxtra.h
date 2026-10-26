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

#ifndef DIRECTOR_LINGO_XTRAS_CAST_FLASHXTRA_H
#define DIRECTOR_LINGO_XTRAS_CAST_FLASHXTRA_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/rect.h"
#include "common/str.h"

#include "director/castmember/castmember.h"

namespace Director {

class XtraCastMember;

namespace FlashXtra {

CastMember *createCastMember(Cast *cast, uint16 castId, XtraCastMember *xtra);

// A tiny SWF (version 1-3) movie: shapes, text and a timeline of
// place/remove tags. Enough for the vector art shipped with early
// Director 6 titles that used the Flash Asset Xtra.
class SWFMovie {
public:
	struct Matrix {
		float a, b, c, d, tx, ty;
		Matrix() : a(1), b(0), c(0), d(1), tx(0), ty(0) {}
		Matrix operator*(const Matrix &o) const;	// apply o first, then this
		Matrix inverse() const;
		void apply(float x, float y, float &ox, float &oy) const {
			ox = a * x + c * y + tx;
			oy = b * x + d * y + ty;
		}
	};

	struct ColorXform {
		float mr, mg, mb, ar, ag, ab;
		ColorXform() : mr(1), mg(1), mb(1), ar(0), ag(0), ab(0) {}
	};

	struct FillStyle {
		int type;			// 0 solid, 1 linear, 2 radial, 3 bitmap
		byte r, g, b, a;
		Matrix matrix;
		Common::Array<byte> ratios;
		Common::Array<byte> colors;	// rgb triplets
		FillStyle() : type(0), r(0), g(0), b(0), a(255) {}
	};

	struct LineStyle {
		float width;		// twips
		byte r, g, b;
		LineStyle() : width(20), r(0), g(0), b(0) {}
	};

	struct Edge {
		float x0, y0, cx, cy, x1, y1;
		bool curve;
		int fill0, fill1, line;
	};

	struct Shape {
		Common::Array<FillStyle> fills;
		Common::Array<LineStyle> lines;
		Common::Array<Edge> edges;
	};

	struct TextGlyph {
		int font;
		int glyph;
		float x, y, height;
		byte r, g, b;
	};

	struct Text {
		Matrix matrix;
		Common::Array<TextGlyph> glyphs;
	};

	struct Placement {
		int id;
		int depth;
		Matrix matrix;
		ColorXform cxform;
	};

	SWFMovie() : _xmin(0), _ymin(0), _xmax(0), _ymax(0), _bgR(255), _bgG(255), _bgB(255), _frameCount(0) {}

	bool load(const byte *data, uint32 size);

	int getFrameCount() const { return _frameCount; }
	Common::Rect getStageRect() const { return Common::Rect((_xmax - _xmin + 19) / 20, (_ymax - _ymin + 19) / 20); }

	// Render a frame (0-based) scaled to width x height; pixels are RGBA8.
	void render(int frame, int width, int height, Common::Array<byte> &rgba, const Matrix *stageOverride = nullptr) const;

private:
	friend class Rasterizer;

	typedef Common::Array<Placement> DisplayList;

	int _xmin, _ymin, _xmax, _ymax;
	byte _bgR, _bgG, _bgB;
	int _frameCount;

	Common::HashMap<int, Shape> _shapes;
	Common::HashMap<int, Text> _texts;
	Common::HashMap<int, Shape> _fonts;			// unused
	Common::HashMap<int, Common::Array<Shape> > _fontGlyphs;
	Common::Array<DisplayList> _frames;			// cumulative display list after each ShowFrame
};

} // End of namespace FlashXtra

class FlashXtraCastMember : public CastMember {
public:
	FlashXtraCastMember(Cast *cast, uint16 castId, XtraCastMember &source);
	FlashXtraCastMember(Cast *cast, uint16 castId, FlashXtraCastMember &source);

	CastMember *duplicate(Cast *cast, uint16 castId) override { return (CastMember *)(new FlashXtraCastMember(cast, castId, *this)); }

	void load() override;
	Common::Point getRegistrationOffset() override { return Common::Point((int)_regX, (int)_regY); }
	Common::Point getRegistrationOffset(int16 w, int16 h) override { return Common::Point((int)(_regX * w / MAX((int16)1, _initialRect.width())), (int)(_regY * h / MAX((int16)1, _initialRect.height()))); }

	Graphics::MacWidget *createWidget(Common::Rect &bbox, Channel *channel, SpriteType spriteType) override;

	bool hasField(int field) override;
	Datum getField(int field) override;
	void setField(int field, const Datum &value) override;

	Common::String formatInfo() override;

	FlashXtra::SWFMovie _movie;
	bool _valid;
	int _frame;
	bool _viewSet;
	float _viewH, _viewV, _viewScale;
	bool _originCenter;
	float _regX, _regY;
};

} // End of namespace Director

#endif
