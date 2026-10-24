/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "hoursgram/music/music_playlists_box.h"

#include "base/unique_qptr.h"
#include "data/data_audio_msg_id.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "hoursgram/music/music_playlists.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "media/player/media_player_instance.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/ui_utility.h"
#include "window/window_session_controller.h"

#include "styles/style_boxes.h"
#include "styles/style_hoursgram_music.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

#include <QtGui/QCursor>

namespace HoursGram::Music {
namespace {

[[nodiscard]] QString TrackText(const PlaylistTrack &track) {
	const auto title = track.title.isEmpty()
		? tr::hoursgram_PlayerUnknownTrack(tr::now)
		: track.title;
	return track.performer.isEmpty()
		? title
		: (track.performer + u" — "_q + title);
}

// A playlist only remembers which message a track came from. The file can
// be played once that document is known to this account; until then the
// chat it was added from has to be opened once.
[[nodiscard]] AudioMsgId ResolveTrack(
		not_null<Main::Session*> session,
		const PlaylistTrack &track) {
	const auto document = session->data().document(track.documentId);
	if (!document->hasRemoteLocation() && document->filepath().isEmpty()) {
		return AudioMsgId();
	}
	return AudioMsgId(document, FullMsgId(track.peerId, track.msgId));
}

void PromptName(
		not_null<Window::SessionController*> controller,
		rpl::producer<QString> title,
		const QString &initial,
		Fn<void(QString)> done) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::duplicate(title));
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			tr::hoursgram_PlaylistName(),
			initial));
		box->setFocusCallback([=] {
			field->setFocusFast();
		});
		const auto save = [=] {
			const auto value = field->getLastText().trimmed();
			if (value.isEmpty()) {
				field->showError();
				return;
			}
			done(value);
			box->closeBox();
		};
		field->submits() | rpl::on_next([=] {
			save();
		}, field->lifetime());
		box->addButton(tr::lng_settings_save(), save);
		box->addButton(tr::lng_cancel(), [=] {
			box->closeBox();
		});
	}));
}

} // namespace

void ShowPlaylists(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::hoursgram_Playlists());
		box->setWidth(st::boxWidth);

		struct State {
			base::unique_qptr<Ui::PopupMenu> menu;
		};
		const auto state = box->lifetime().make_state<State>();
		const auto session = &controller->session();
		const auto account = AccountOf(session);
		const auto container = box->verticalLayout();
		const auto inner = container->add(
			object_ptr<Ui::VerticalLayout>(container));

		const auto rebuild = [=] {
			inner->clear();
			const auto list = Playlists::Instance().list(account);
			if (list.empty()) {
				inner->add(
					object_ptr<Ui::FlatLabel>(
						inner,
						tr::hoursgram_PlaylistsEmpty(),
						st::hoursgramPlayerPerformer),
					st::hoursgramPlaylistEmptyPadding);
				return;
			}
			for (const auto &playlist : list) {
				const auto id = playlist.id;
				const auto row = inner->add(
					object_ptr<Ui::SettingsButton>(
						inner,
						rpl::single(playlist.title),
						st::infoProfileButton));
				row->setClickedCallback([=] {
					ShowPlaylist(controller, id);
				});
				row->setAcceptBoth(true);
				row->clicks() | rpl::on_next([=](Qt::MouseButton button) {
					if (button != Qt::RightButton) {
						return;
					}
					state->menu = base::make_unique_q<Ui::PopupMenu>(
						row,
						st::popupMenuWithIcons);
					const auto menu = state->menu.get();
					menu->addAction(tr::hoursgram_PlaylistRename(tr::now), [=] {
						PromptName(
							controller,
							tr::hoursgram_PlaylistRename(),
							playlist.title,
							[=](QString name) {
								Playlists::Instance().rename(
									account,
									id,
									name);
							});
					}, &st::menuIconEdit);
					menu->addAction(tr::hoursgram_PlaylistDelete(tr::now), [=] {
						Playlists::Instance().remove(account, id);
					}, &st::menuIconDelete);
					menu->popup(QCursor::pos());
				}, row->lifetime());
			}
		};
		rebuild();

		Playlists::Instance().changes() | rpl::on_next([=] {
			rebuild();
		}, box->lifetime());

		box->addLeftButton(tr::hoursgram_PlaylistNew(), [=] {
			PromptName(
				controller,
				tr::hoursgram_PlaylistNew(),
				QString(),
				[=](QString name) {
					Playlists::Instance().create(account, name);
				});
		});
		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowPlaylist(
		not_null<Window::SessionController*> controller,
		uint64 playlistId) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		struct State {
			base::unique_qptr<Ui::PopupMenu> menu;
		};
		const auto state = box->lifetime().make_state<State>();
		const auto session = &controller->session();
		const auto account = AccountOf(session);
		const auto container = box->verticalLayout();
		const auto inner = container->add(
			object_ptr<Ui::VerticalLayout>(container));

		box->setWidth(st::boxWidth);

		const auto rebuild = [=] {
			const auto playlist = Playlists::Instance().playlist(
				account,
				playlistId);
			if (!playlist) {
				box->closeBox();
				return;
			}
			box->setTitle(rpl::single(playlist->title));
			inner->clear();
			if (playlist->tracks.empty()) {
				inner->add(
					object_ptr<Ui::FlatLabel>(
						inner,
						tr::hoursgram_PlaylistEmpty(),
						st::hoursgramPlayerPerformer),
					st::hoursgramPlaylistEmptyPadding);
				return;
			}
			for (const auto &track : playlist->tracks) {
				const auto documentId = track.documentId;
				const auto row = inner->add(
					object_ptr<Ui::SettingsButton>(
						inner,
						rpl::single(TrackText(track)),
						st::infoProfileButton));
				row->setAcceptBoth(true);
				row->clicks() | rpl::on_next([=](Qt::MouseButton button) {
					if (button == Qt::RightButton) {
						state->menu = base::make_unique_q<Ui::PopupMenu>(
							row,
							st::popupMenuWithIcons);
						const auto menu = state->menu.get();
						menu->addAction(
							tr::hoursgram_PlaylistRemoveTrack(tr::now),
							[=] {
								Playlists::Instance().removeTrack(
									account,
									playlistId,
									documentId);
							},
							&st::menuIconDelete);
						menu->popup(QCursor::pos());
						return;
					}
					const auto audioId = ResolveTrack(session, track);
					if (!audioId.audio()) {
						controller->showToast(
							tr::hoursgram_PlaylistTrackMissing(tr::now));
						return;
					}
					Media::Player::instance()->play(audioId);
				}, row->lifetime());
			}
		};
		rebuild();

		Playlists::Instance().changes() | rpl::on_next([=] {
			rebuild();
		}, box->lifetime());

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowAddToPlaylist(
		not_null<Window::SessionController*> controller,
		const AudioMsgId &audioId) {
	const auto document = audioId.audio();
	if (!document) {
		return;
	}
	auto track = PlaylistTrack();
	track.documentId = document->id;
	track.peerId = audioId.contextId().peer;
	track.msgId = audioId.contextId().msg;
	if (const auto song = document->song()) {
		track.title = song->title;
		track.performer = song->performer;
	}
	if (track.title.isEmpty()) {
		track.title = document->filename();
	}

	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::hoursgram_AddToPlaylist());
		box->setWidth(st::boxWidth);

		const auto session = &controller->session();
		const auto account = AccountOf(session);
		const auto container = box->verticalLayout();
		const auto inner = container->add(
			object_ptr<Ui::VerticalLayout>(container));

		const auto add = [=](uint64 playlistId, const QString &name) {
			if (Playlists::Instance().addTrack(account, playlistId, track)) {
				controller->showToast(tr::hoursgram_PlaylistAdded(
					tr::now,
					lt_name,
					name));
			} else {
				controller->showToast(tr::hoursgram_PlaylistAlready(
					tr::now,
					lt_name,
					name));
			}
			box->closeBox();
		};

		const auto rebuild = [=] {
			inner->clear();
			const auto list = Playlists::Instance().list(account);
			if (list.empty()) {
				inner->add(
					object_ptr<Ui::FlatLabel>(
						inner,
						tr::hoursgram_PlaylistsEmpty(),
						st::hoursgramPlayerPerformer),
					st::hoursgramPlaylistEmptyPadding);
				return;
			}
			for (const auto &playlist : list) {
				const auto id = playlist.id;
				const auto name = playlist.title;
				const auto row = inner->add(
					object_ptr<Ui::SettingsButton>(
						inner,
						rpl::single(name),
						st::infoProfileButton));
				row->setClickedCallback([=] {
					add(id, name);
				});
			}
		};
		rebuild();

		Playlists::Instance().changes() | rpl::on_next([=] {
			rebuild();
		}, box->lifetime());

		box->addLeftButton(tr::hoursgram_PlaylistNew(), [=] {
			PromptName(
				controller,
				tr::hoursgram_PlaylistNew(),
				QString(),
				[=](QString name) {
					const auto id = Playlists::Instance().create(
						account,
						name);
					if (id) {
						add(id, name);
					}
				});
		});
		box->addButton(tr::lng_cancel(), [=] {
			box->closeBox();
		});
	}));
}

} // namespace HoursGram::Music
