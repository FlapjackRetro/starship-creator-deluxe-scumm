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

#ifndef DIRECTOR_LINGO_XTRAS_CAST_SWAXTRA_H
#define DIRECTOR_LINGO_XTRAS_CAST_SWAXTRA_H

#include "director/castmember/castmember.h"

namespace Director {

class XtraCastMember;

// Shockwave Audio (SWA) streaming cast member.
//
// An .swa file is a small Macromedia header (the first big-endian 32-bit
// word is the header length, typically 0x140) followed by a plain MPEG audio
// stream. Lingo drives the member with `the URL`, `the state`,
// `preLoadBuffer`, `play` and `stop`.
class SWAXtraCastMember : public CastMember {
public:
	// `the state of member` values, as documented for SWA
	enum State {
		kStateStopped = 0,
		kStatePreloading = 1,
		kStatePreloaded = 2,
		kStatePlaying = 3,
		kStatePaused = 4,
		kStateDone = 5,
		kStateError = 9
	};

	SWAXtraCastMember(Cast *cast, uint16 castId, XtraCastMember &source);
	SWAXtraCastMember(Cast *cast, uint16 castId, SWAXtraCastMember &source);
	~SWAXtraCastMember() override;

	CastMember *duplicate(Cast *cast, uint16 castId) override { return (CastMember *)(new SWAXtraCastMember(cast, castId, *this)); }

	bool isSWA() const override { return true; }

	bool hasField(int field) override;
	Datum getField(int field) override;
	void setField(int field, const Datum &value) override;

	Common::String formatInfo() override;

	void preLoadBuffer();
	void play();
	void stop();
	int getState();

private:
	Common::SeekableReadStream *openStream();

	Common::String _url;
	int _state;
	int _preLoadTime;
	bool _playing;
	bool _fileOk;
};

namespace SWAXtra {

CastMember *createCastMember(Cast *cast, uint16 castId, XtraCastMember *xtra);
// Returns the SWA member a `member x` Datum refers to, or nullptr
SWAXtraCastMember *fromDatum(const Datum &d);

} // End of namespace SWAXtra

} // End of namespace Director

#endif
