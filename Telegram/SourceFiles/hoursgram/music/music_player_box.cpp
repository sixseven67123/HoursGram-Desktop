/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "hoursgram/music/music_player_box.h"

#include "core/file_utilities.h"
#include "data/data_document.h"
#include "hoursgram/music/music_lyrics_loader.h"
#include "hoursgram/music/music_playlists_box.h"
#include "lang/lang_keys.h"
#include "media/audio/media_audio.h"
#include "media/player/media_player_instance.h"
#include "ui/effects/animations.h"
#include "ui/layers/generic_box.h"
#include "ui/text/format_values.h"
#include "ui/text/text_options.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "ui/vertical_list.h"
#include "window/window_session_controller.h"

#include "styles/style_hoursgram_music.h"
#include "styles/style_layers.h"

#include <QtCore/QFile>

namespace HoursGram::Music {
namespace {

constexpr auto kType = AudioMsgId::Type::Song;
constexpr auto kScrollDuration = crl::time(350);

[[nodiscard]] Media::Player::Instance *Player() {
	return Media::Player::instance();
}

// The cover of a track we have no artwork for: a soft accent gradient with
// the player glyph in the middle, drawn with the same corner radius as the
// rest of the window.
class Cover final : public Ui::RpWidget {
public:
	explicit Cover(QWidget *parent);

protected:
	void paintEvent(QPaintEvent *e) override;

};

Cover::Cover(QWidget *parent) : RpWidget(parent) {
	resize(st::hoursgramPlayerCoverSize, st::hoursgramPlayerCoverSize);
}

void Cover::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);

	const auto size = st::hoursgramPlayerCoverSize;
	const auto inner = QRect((width() - size) / 2, 0, size, size);
	auto gradient = QLinearGradient(inner.topLeft(), inner.bottomRight());
	gradient.setColorAt(0., st::windowBgActive->c);
	gradient.setColorAt(1., st::windowBgRipple->c);
	p.setPen(Qt::NoPen);
	p.setBrush(gradient);
	p.drawRoundedRect(
		inner,
		st::hoursgramPlayerCoverRadius,
		st::hoursgramPlayerCoverRadius);

	const auto &icon = st::hoursgramPlayerCoverIcon;
	icon.paint(
		p,
		inner.x() + (size - icon.width()) / 2,
		inner.y() + (size - icon.height()) / 2,
		width());
}

// Draws the lyrics lines one under another, highlighting the line that the
// playback is inside of and telling the scroll area where it sits.
class LyricsInner final : public Ui::RpWidget {
public:
	explicit LyricsInner(QWidget *parent);

	void setLyrics(Lyrics lyrics);
	void setPosition(crl::time position);

	[[nodiscard]] bool empty() const;
	[[nodiscard]] rpl::producer<int> activeCenterChanges() const;

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;

private:
	struct Line {
		Ui::Text::String text;
		crl::time at = -1;
		int top = 0;
		int height = 0;
	};

	void setActive(int index);

	Lyrics _lyrics;
	std::vector<Line> _lines;
	rpl::event_stream<int> _activeCenterChanges;
	int _active = -1;

};

LyricsInner::LyricsInner(QWidget *parent) : RpWidget(parent) {
}

bool LyricsInner::empty() const {
	return _lines.empty();
}

rpl::producer<int> LyricsInner::activeCenterChanges() const {
	return _activeCenterChanges.events();
}

void LyricsInner::setLyrics(Lyrics lyrics) {
	_lyrics = std::move(lyrics);
	_lines.clear();
	_lines.reserve(_lyrics.lines.size());
	for (const auto &line : _lyrics.lines) {
		auto entry = Line();
		entry.at = line.at;
		entry.text.setText(
			st::hoursgramLyricsActive,
			line.text,
			Ui::NameTextOptions());
		_lines.push_back(std::move(entry));
	}
	_active = -1;
	resizeToWidth(width());
	update();
}

void LyricsInner::setPosition(crl::time position) {
	setActive(_lyrics.lineAt(position));
}

void LyricsInner::setActive(int index) {
	if (_active == index) {
		return;
	}
	_active = index;
	update();
	if (_active >= 0 && _active < int(_lines.size())) {
		const auto &line = _lines[_active];
		_activeCenterChanges.fire(line.top + (line.height / 2));
	}
}

int LyricsInner::resizeGetHeight(int newWidth) {
	const auto padding = st::hoursgramLyricsPadding;
	const auto available = newWidth - padding.left() - padding.right();
	if (available <= 0) {
		return height();
	}
	auto top = padding.top();
	for (auto &line : _lines) {
		line.top = top;
		line.height = line.text.isEmpty()
			? st::hoursgramLyricsActive.font->height
			: line.text.countHeight(available);
		top += line.height + st::hoursgramLyricsLineSkip;
	}
	if (!_lines.empty()) {
		top -= st::hoursgramLyricsLineSkip;
	}
	return top + padding.bottom();
}

void LyricsInner::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	const auto padding = st::hoursgramLyricsPadding;
	const auto available = width() - padding.left() - padding.right();
	if (available <= 0) {
		return;
	}
	const auto clip = e->rect();
	const auto bottom = clip.y() + clip.height();
	for (auto i = 0, count = int(_lines.size()); i != count; ++i) {
		const auto &line = _lines[i];
		if (line.top + line.height < clip.y()) {
			continue;
		} else if (line.top > bottom) {
			break;
		} else if (line.text.isEmpty()) {
			continue;
		}
		const auto active = (i == _active);
		p.setOpacity(active ? 1. : 0.6);
		p.setPen(active ? st::windowBgActive : st::windowSubTextFg);
		line.text.drawLeft(
			p,
			padding.left(),
			line.top,
			available,
			width(),
			style::al_top);
	}
	p.setOpacity(1.);
}

} // namespace

void ShowPlayer(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		struct State {
			Ui::Animations::Simple scroll;
			DocumentId lyricsFor = 0;
			crl::time durationMs = 0;
			crl::time seekPositionMs = -1;
			int scrollFrom = 0;
			int scrollTo = 0;
			bool over = false;
		};
		const auto state = box->lifetime().make_state<State>();

		box->setTitle(tr::hoursgram_PlayerTitle());
		box->setWidth(st::hoursgramPlayerWidth);

		const auto layout = box->verticalLayout();
		const auto padding = st::boxRowPadding;

		Ui::AddSkip(layout, st::hoursgramPlayerCoverSkip);
		layout->add(
			object_ptr<Cover>(box),
			style::margins(0, 0, 0, st::hoursgramPlayerCoverSkip));

		const auto title = layout->add(
			object_ptr<Ui::FlatLabel>(box, st::hoursgramPlayerTitle),
			padding);
		Ui::AddSkip(layout, st::hoursgramPlayerPerformerSkip);
		const auto performer = layout->add(
			object_ptr<Ui::FlatLabel>(box, st::hoursgramPlayerPerformer),
			padding);

		Ui::AddSkip(layout, st::hoursgramPlayerSliderSkip);
		auto sliderOwned = object_ptr<Ui::MediaSlider>(
			box,
			st::hoursgramPlayerSlider);
		sliderOwned->resize(
			st::hoursgramPlayerWidth,
			st::hoursgramPlayerSliderHeight);
		const auto slider = layout->add(std::move(sliderOwned), padding);

		Ui::AddSkip(layout, st::hoursgramPlayerTimeSkip);
		const auto times = layout->add(
			object_ptr<Ui::FixedHeightWidget>(
				box,
				st::hoursgramPlayerTime.style.font->height),
			padding);
		const auto position = Ui::CreateChild<Ui::FlatLabel>(
			times,
			st::hoursgramPlayerTime);
		const auto duration = Ui::CreateChild<Ui::FlatLabel>(
			times,
			st::hoursgramPlayerTime);
		times->widthValue() | rpl::on_next([=](int width) {
			position->moveToLeft(0, 0, width);
			duration->moveToRight(0, 0, width);
		}, times->lifetime());
		duration->widthValue() | rpl::on_next([=] {
			duration->moveToRight(0, 0, times->width());
		}, duration->lifetime());

		Ui::AddSkip(layout, st::hoursgramPlayerControlsSkip);
		const auto controls = layout->add(
			object_ptr<Ui::FixedHeightWidget>(
				box,
				st::hoursgramPlayerControlsHeight));
		const auto previous = Ui::CreateChild<Ui::IconButton>(
			controls,
			st::hoursgramPlayerPrevious);
		const auto play = Ui::CreateChild<Ui::IconButton>(
			controls,
			st::hoursgramPlayerPlay);
		const auto next = Ui::CreateChild<Ui::IconButton>(
			controls,
			st::hoursgramPlayerSide);
		controls->widthValue() | rpl::on_next([=](int width) {
			const auto space = st::hoursgramPlayerControlsSpace;
			const auto center = width / 2;
			play->moveToLeft(
				center - (play->width() / 2),
				(controls->height() - play->height()) / 2,
				width);
			previous->moveToLeft(
				center - (play->width() / 2) - space - previous->width(),
				(controls->height() - previous->height()) / 2,
				width);
			next->moveToLeft(
				center + (play->width() / 2) + space,
				(controls->height() - next->height()) / 2,
				width);
		}, controls->lifetime());
		controls->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(controls);
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowBgActive);
			p.drawEllipse(play->geometry());
		}, controls->lifetime());

		Ui::AddSkip(layout, st::hoursgramLyricsSkip);
		auto scrollOwned = object_ptr<Ui::ScrollArea>(
			box,
			st::defaultScrollArea);
		scrollOwned->resize(
			st::hoursgramPlayerWidth,
			st::hoursgramLyricsHeight);
		const auto scroll = layout->add(std::move(scrollOwned));
		auto lyricsOwned = object_ptr<LyricsInner>(scroll);
		const auto lyrics = lyricsOwned.data();
		scroll->setOwnedWidget(std::move(lyricsOwned));
		const auto empty = layout->add(
			object_ptr<Ui::FlatLabel>(
				box,
				tr::hoursgram_LyricsEmpty(),
				st::hoursgramPlayerPerformer),
			st::hoursgramLyricsEmptyPadding);

		lyrics->activeCenterChanges() | rpl::on_next([=](int center) {
			const auto height = scroll->height();
			const auto maximum = std::max(lyrics->height() - height, 0);
			const auto wanted = std::clamp(
				center - (height / 2),
				0,
				maximum);
			state->scrollFrom = scroll->scrollTop();
			state->scrollTo = wanted;
			state->scroll.stop();
			state->scroll.start([=](float64 value) {
				scroll->scrollToY(int(base::SafeRound(value)));
			}, state->scrollFrom, state->scrollTo, kScrollDuration);
		}, box->lifetime());

		const auto showLyrics = [=](bool has) {
			scroll->setVisible(has);
			empty->setVisible(!has);
		};

		const auto reloadLyrics = [=](DocumentData *document) {
			const auto id = document ? document->id : DocumentId();
			state->lyricsFor = id;
			auto loaded = document ? LoadLyrics(document) : Lyrics();
			const auto has = !loaded.empty();
			lyrics->setLyrics(std::move(loaded));
			showLyrics(has);
		};

		const auto updateTrack = [=](const AudioMsgId &audioId) {
			const auto document = audioId.audio();
			if (!document) {
				title->setText(tr::hoursgram_PlayerIdle(tr::now));
				performer->setText(QString());
				reloadLyrics(nullptr);
				return;
			}
			const auto song = document->song();
			const auto name = (song && !song->title.isEmpty())
				? song->title
				: document->filename();
			title->setText(name);
			performer->setText((song && !song->performer.isEmpty())
				? song->performer
				: tr::hoursgram_PlayerUnknownArtist(tr::now));
			if (state->lyricsFor != document->id) {
				reloadLyrics(document);
			}
		};

		const auto updateState = [=](const Media::Player::TrackState &track) {
			const auto frequency = track.frequency
				? track.frequency
				: Media::Player::kDefaultFrequency;
			state->durationMs = (track.length * 1000LL) / frequency;
			const auto positionMs = (track.position * 1000LL) / frequency;
			if (state->seekPositionMs < 0) {
				position->setText(
					Ui::FormatDurationText(positionMs / 1000));
				slider->setValue(state->durationMs
					? (float64(positionMs) / state->durationMs)
					: 0.);
				lyrics->setPosition(positionMs);
			}
			duration->setText(Ui::FormatDurationText(
				state->durationMs / 1000));
			slider->setDisabled(!state->durationMs);
			play->setIconOverride(
				Media::Player::ShowPauseIcon(track.state)
					? &st::hoursgramPlayerPauseIcon
					: nullptr);
		};

		slider->setChangeProgressCallback([=](float64 value) {
			if (!state->durationMs) {
				return;
			}
			state->seekPositionMs = crl::time(value * state->durationMs);
			position->setText(
				Ui::FormatDurationText(state->seekPositionMs / 1000));
			lyrics->setPosition(state->seekPositionMs);
			Player()->startSeeking(kType);
		});
		slider->setChangeFinishedCallback([=](float64 value) {
			state->seekPositionMs = -1;
			if (state->durationMs) {
				Player()->finishSeeking(kType, value);
			} else {
				Player()->cancelSeeking(kType);
			}
		});

		play->setClickedCallback([=] {
			Player()->playPause(kType);
		});
		previous->setClickedCallback([=] {
			Player()->previous(kType);
		});
		next->setClickedCallback([=] {
			Player()->next(kType);
		});

		Player()->updatedNotifier(
		) | rpl::filter([](const Media::Player::TrackState &track) {
			return (track.id.type() == kType);
		}) | rpl::on_next([=](const Media::Player::TrackState &track) {
			updateTrack(track.id);
			updateState(track);
		}, box->lifetime());

		Player()->trackChanged(
		) | rpl::filter([](AudioMsgId::Type type) {
			return (type == kType);
		}) | rpl::on_next([=] {
			updateTrack(Player()->current(kType));
		}, box->lifetime());

		updateTrack(Player()->current(kType));
		updateState(Player()->getState(kType));

		box->addLeftButton(tr::hoursgram_LyricsAttach(), [=] {
			const auto current = Player()->current(kType);
			const auto document = current.audio();
			if (!document) {
				return;
			}
			const auto id = document->id;
			FileDialog::GetOpenPath(
				box.get(),
				tr::hoursgram_LyricsAttach(tr::now),
				tr::hoursgram_LyricsFilter(tr::now),
				crl::guard(box, [=](FileDialog::OpenResult &&result) {
					auto bytes = result.remoteContent;
					if (bytes.isEmpty() && !result.paths.isEmpty()) {
						auto file = QFile(result.paths.front());
						if (file.open(QIODevice::ReadOnly)) {
							bytes = file.readAll();
						}
					}
					if (AttachLyrics(id, bytes)) {
						reloadLyrics(Player()->current(kType).audio());
					}
				}));
		});

		box->addButton(tr::hoursgram_AddToPlaylist(), [=] {
			const auto current = Player()->current(kType);
			if (current.audio()) {
				ShowAddToPlaylist(controller, current);
			}
		});
		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

} // namespace HoursGram::Music
