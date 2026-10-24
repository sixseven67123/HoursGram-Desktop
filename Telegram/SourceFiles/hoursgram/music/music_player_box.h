/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace HoursGram::Music {

// The full player screen: cover, playback controls and the lyrics of the
// track that is playing, following the playback position when the lyrics
// file carries timings.
void ShowPlayer(not_null<Window::SessionController*> controller);

} // namespace HoursGram::Music
