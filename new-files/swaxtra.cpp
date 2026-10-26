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

#include "common/macresman.h"
#include "common/substream.h"
#include "audio/audiostream.h"
#include "audio/decoders/mp3.h"

#include "director/director.h"
#include "director/cast.h"
#include "director/movie.h"
#include "director/sound.h"
#include "director/util.h"
#include "director/window.h"
#include "director/castmember/xtra.h"
#include "director/lingo/lingo.h"
#include "director/lingo/lingo-the.h"
#include "director/lingo/xtras-cast/swaxtra.h"

namespace Director {

// SWA streams play on their own sound channel
static const int kSWASoundChannel = 4;

namespace SWAXtra {

CastMember *createCastMember(Cast *cast, uint16 castId, XtraCastMember *xtra) {
	return new SWAXtraCastMember(cast, castId, *xtra);
}

SWAXtraCastMember *fromDatum(const Datum &d) {
	if (d.type != CASTREF || !d.u.cast)
		return nullptr;

	Movie *movie = g_director->getCurrentMovie();
	if (!movie)
		return nullptr;

	CastMember *member = movie->getCastMember(*d.u.cast);
	if (!member || !member->isSWA())
		return nullptr;

	return static_cast<SWAXtraCastMember *>(member);
}

} // End of namespace SWAXtra

SWAXtraCastMember::SWAXtraCastMember(Cast *cast, uint16 castId, XtraCastMember &source)
		: CastMember(cast, castId), _state(kStateStopped), _preLoadTime(0), _playing(false), _fileOk(false) {
	_type = kCastXtra;
}

SWAXtraCastMember::SWAXtraCastMember(Cast *cast, uint16 castId, SWAXtraCastMember &source)
		: CastMember(cast, castId), _state(kStateStopped), _preLoadTime(source._preLoadTime), _playing(false), _fileOk(false) {
	_type = kCastXtra;
	_url = source._url;
	if (cast == source._cast)
		_children = source._children;
}

SWAXtraCastMember::~SWAXtraCastMember() {
	if (_playing)
		stop();
}

bool SWAXtraCastMember::hasField(int field) {
	switch (field) {
	case kTheURL:
	case kTheState:
	case kThePreLoadTime:
		return true;
	default:
		break;
	}
	return CastMember::hasField(field);
}

Datum SWAXtraCastMember::getField(int field) {
	switch (field) {
	case kTheURL:
		return Datum(_url);
	case kTheState:
		return Datum(getState());
	case kThePreLoadTime:
		return Datum(_preLoadTime);
	default:
		break;
	}
	return CastMember::getField(field);
}

void SWAXtraCastMember::setField(int field, const Datum &d) {
	switch (field) {
	case kTheURL:
		if (_playing)
			stop();
		_url = d.asString();
		_state = kStateStopped;
		_fileOk = false;
		debugC(2, kDebugLoading, "SWAXtraCastMember: url of member %d set to '%s'", _castId, _url.c_str());
		return;
	case kThePreLoadTime:
		_preLoadTime = d.asInt();
		return;
	case kTheState:
		// read-only
		return;
	default:
		break;
	}
	CastMember::setField(field, d);
}

Common::String SWAXtraCastMember::formatInfo() {
	return Common::String::format("SWA Xtra: url: '%s', state: %d", _url.c_str(), _state);
}

// Opens the .swa file and returns a stream positioned on the MPEG data
Common::SeekableReadStream *SWAXtraCastMember::openStream() {
	if (_url.empty())
		return nullptr;

	// The URL is an absolute path as seen by the game (e.g. "C:\\Media\\Audio\\x.swa"),
	// so resolve it against the game directory the same way Lingo file paths are.
	Common::Path path = findAbsolutePath(_url);
	Common::SeekableReadStream *file = Common::MacResManager::openFileOrDataFork(path);
	if (!file) {
		path = findAudioPath(_url);
		file = Common::MacResManager::openFileOrDataFork(path);
	}
	if (!file) {
		warning("SWAXtraCastMember: failed to open '%s' (resolved to '%s')", _url.c_str(), path.toString().c_str());
		return nullptr;
	}

	if (file->size() < 8) {
		delete file;
		return nullptr;
	}

	// The first big-endian word is the length of the SWA header
	uint32 headerLen = file->readUint32BE();
	if (headerLen < 8 || headerLen >= (uint32)file->size()) {
		warning("SWAXtraCastMember: unexpected header length %u in '%s'", headerLen, _url.c_str());
		delete file;
		return nullptr;
	}

	return new Common::SeekableSubReadStream(file, headerLen, file->size(), DisposeAfterUse::YES);
}

void SWAXtraCastMember::preLoadBuffer() {
	if (_playing)
		return;

	Common::SeekableReadStream *s = openStream();
	_fileOk = s != nullptr;
	delete s;

	_state = _fileOk ? kStatePreloaded : kStateError;
}

void SWAXtraCastMember::play() {
	DirectorSound *sound = g_director->getCurrentWindow()->getSoundManager();
	if (!sound)
		return;

	Common::SeekableReadStream *s = openStream();
	if (!s) {
		_state = kStateError;
		return;
	}

#ifdef USE_MAD
	Audio::SeekableAudioStream *audio = Audio::makeMP3Stream(s, DisposeAfterUse::YES);
	if (!audio) {
		warning("SWAXtraCastMember: could not decode '%s'", _url.c_str());
		_state = kStateError;
		return;
	}

	sound->playStream(*audio, kSWASoundChannel);
	_playing = true;
	_state = kStatePlaying;
#else
	warning("STUB: SWAXtraCastMember::play(): ScummVM was built without MP3 support, cannot play '%s'", _url.c_str());
	delete s;
	_state = kStateError;
#endif
}

void SWAXtraCastMember::stop() {
	if (_playing) {
		DirectorSound *sound = g_director->getCurrentWindow()->getSoundManager();
		if (sound)
			sound->stopSound(kSWASoundChannel);
	}
	_playing = false;
	_state = kStateStopped;
}

int SWAXtraCastMember::getState() {
	if (_playing) {
		DirectorSound *sound = g_director->getCurrentWindow()->getSoundManager();
		if (sound && sound->isChannelActive(kSWASoundChannel))
			return kStatePlaying;
		// the stream finished (or another sound took the channel)
		_playing = false;
		_state = kStateDone;
	}
	return _state;
}

} // End of namespace Director
