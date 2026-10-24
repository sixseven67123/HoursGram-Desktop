// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/settings_appearance.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ayu_ui_settings.h"
#include "ayu/ui/boxes/font_selector.h"
#include "ayu/ui/components/avatar_corners_preview.h"
#include "ayu/ui/components/icon_picker.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "ayu/ui/settings/settings_main.h"
#include "boxes/star_gift_box.h"
#include "data/data_user.h"
#include "info/peer_gifts/info_peer_gifts_common.h"
#include "inline_bots/bot_attach_web_view.h"
#include "main/main_session.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_icons.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_dialogs.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <array>

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

struct LocalGiftOffer {
	QString id;
	int price;
};

const auto LOCAL_GIFT_OFFERS = std::array{
	LocalGiftOffer{ QStringLiteral("teddy"), 15 },
	LocalGiftOffer{ QStringLiteral("cookie"), 25 },
	LocalGiftOffer{ QStringLiteral("lollipop"), 15 },
	LocalGiftOffer{ QStringLiteral("flamingo"), 20 },
	LocalGiftOffer{ QStringLiteral("backpack"), 30 },
	LocalGiftOffer{ QStringLiteral("lamp"), 35 },
};

constexpr auto LOCAL_STARS_GRANT = 100;

[[nodiscard]] rpl::producer<QString> GetLocalGiftTitle(const QString &giftId) {
	if (giftId == u"teddy"_q) {
		return tr::hoursgram_GiftTeddy();
	} else if (giftId == u"cookie"_q) {
		return tr::hoursgram_GiftCookieHeart();
	} else if (giftId == u"lollipop"_q) {
		return tr::hoursgram_GiftLollipop();
	} else if (giftId == u"flamingo"_q) {
		return tr::hoursgram_GiftFlamingo();
	} else if (giftId == u"backpack"_q) {
		return tr::hoursgram_GiftBackpack();
	} else if (giftId == u"lamp"_q) {
		return tr::hoursgram_GiftLantern();
	}
	Unexpected("Unknown local HoursGram gift id.");
}

void ShowLocalProfileTextBox(
		not_null<Window::SessionController*> controller,
		rpl::producer<QString> title,
		rpl::producer<QString> placeholder,
		QString current,
		Fn<void(QString)> saveValue) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(title);
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::NoNewlines,
			placeholder,
			current));
		field->setMaxLength(32);
		box->setFocusCallback([=] { field->setFocusFast(); });
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::hoursgram_LocalOnlyNotice(),
			st::boxDividerLabel));
		const auto submit = [=] {
			saveValue(field->getLastText().trimmed());
			box->closeBox();
		};
		field->submits() | rpl::on_next([=](auto) {
			submit();
		}, field->lifetime());
		box->addButton(tr::lng_settings_save(), submit);
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

void ShowLocalNftNumberBox(
		not_null<Window::SessionController*> controller,
		Fn<void(int)> saveNumber) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::hoursgram_LocalNftNumber());
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::NoNewlines,
			tr::hoursgram_LocalNftNumber(),
			u"1"_q));
		field->setMaxLength(10);
		box->setFocusCallback([=] { field->setFocusFast(); });
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::hoursgram_LocalOnlyNotice(),
			st::boxDividerLabel));
		const auto submit = [=] {
			auto valid = false;
			const auto number = field->getLastText().trimmed().toInt(&valid);
			if (!valid || number <= 0) {
				field->showError();
				return;
			}
			saveNumber(number);
			box->closeBox();
		};
		field->submits() | rpl::on_next([=](auto) {
			submit();
		}, field->lifetime());
		box->addButton(tr::lng_settings_save(), submit);
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

bool HasDrawerBots(not_null<Window::SessionController*> controller) {
	// todo: maybe iterate through all accounts
	const auto bots = &controller->session().attachWebView();
	for (const auto &bot : bots->attachBots()) {
		if (!bot.inMainMenu || !bot.media) {
			continue;
		}
		return true;
	}
	return false;
}

void BuildAppIcon(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle({
		.id = u"ayu/appIcon"_q,
		.title = tr::ayu_AppIconHeader(),
	});

	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		return {
			.widget = object_ptr<IconPicker>(ctx.container),
			.margin = st::settingsButtonNoIcon.padding,
		};
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	builder.addDivider();
	builder.addSkip();
	ayu.addSettingToggle({
		.id = u"ayu/hideNotificationBadge"_q,
		.title = tr::ayu_HideNotificationBadge(),
		.getter = &AyuSettings::hideNotificationBadge,
		.setter = &AyuSettings::setHideNotificationBadge,
	});
	builder.addSkip();
	builder.addDividerText(tr::ayu_HideNotificationBadgeDescription());
	builder.addSkip();
#else
    builder.addDivider();
    builder.addSkip();
#endif
}

void BuildAvatarCorners(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();
	const auto controller = builder.controller();

	const auto mapRadius = [](int val)
	{
		if (val == 0) {
			return tr::ayu_AvatarCornersSquare(tr::now).toUpper();
		} else if (val == AyuUiSettings::kMaxAvatarCorners) {
			return tr::ayu_AvatarCornersCircle(tr::now).toUpper();
		}
		return QString::number(val);
	};

	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		const auto container = ctx.container;
		auto title = object_ptr<Ui::FlatLabel>(
			container,
			tr::ayu_AvatarCorners(),
			st::defaultSubsectionTitle);
		const auto titleRaw = title.data();

		const auto badge = Ui::CreateChild<Ui::PaddingWrap<Ui::FlatLabel>>(
			container,
			object_ptr<Ui::FlatLabel>(
				container,
				settings->avatarCornersValue() | rpl::map(mapRadius),
				st::settingsPremiumNewBadge),
			st::ayuBetaBadgePadding);
		badge->show();
		badge->setAttribute(Qt::WA_TransparentForMouseEvents);
		badge->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(badge);
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowBgActive);
			const auto r = st::ayuBetaBadgePadding.left();
			p.drawRoundedRect(badge->rect(), r, r);
		}, badge->lifetime());

		titleRaw->geometryValue() | rpl::on_next([=](QRect geometry) {
			badge->moveToLeft(
				geometry.x()
					+ titleRaw->textMaxWidth()
					+ st::settingsPremiumNewBadgePosition.x(),
				geometry.y()
					+ (geometry.height() - badge->height()) / 2);
		}, badge->lifetime());

		return {
			.widget = std::move(title),
			.margin = st::defaultSubsectionTitlePadding,
		};
	}, [] {
		return SearchEntry{
			.id = u"ayu/avatarCorners"_q,
			.title = tr::ayu_AvatarCorners(tr::now),
		};
	});

	auto *previewRaw = static_cast<AvatarCornersPreview*>(nullptr);
	builder.add([&](const Builder::WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto preview = object_ptr<AvatarCornersPreview>(
			ctx.container,
			controller);
		previewRaw = preview.data();
		const auto vMargin = st::settingsButtonNoIcon.padding
			- st::defaultDialogRow.padding;
		return {
			.widget = std::move(preview),
			.margin = QMargins(0, vMargin.top(), 0, vMargin.bottom()),
		};
	});

	ayu.addSlider({
		.id = u"ayu/avatarCornersSlider"_q,
		.title = rpl::single(QString()),
		.showTitle = false,
		.steps = AyuUiSettings::kMaxAvatarCorners + 1,
		.current = settings->avatarCorners(),
		.onChanged = [=](int val) {
			AyuSettings::getInstance().setAvatarCorners(val);
			if (previewRaw) {
				previewRaw->update();
			}
		},
		.onFinalChanged = [=](int val) {
			AyuSettings::getInstance().setAvatarCorners(val);
			ShowRestartPrompt(controller);
		},
	});

	ayu.addSettingToggle({
		.id = u"ayu/singleCornerRadius"_q,
		.title = tr::ayu_SingleCornerRadius(),
		.getter = &AyuSettings::singleCornerRadius,
		.setter = &AyuSettings::setSingleCornerRadius,
	});

	builder.addSkip();
	builder.addDividerText(tr::ayu_SingleCornerRadiusDescription());
	builder.addSkip();
}

void BuildAppearance(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	builder.addSubsectionTitle(tr::ayu_CategoryAppearance());
	ayu.addChooseButton({
		.id = u"hoursgram/theme"_q,
		.title = tr::hoursgram_ThemeChoiceTitle(),
		.boxTitle = tr::hoursgram_ThemeChoiceTitle(),
		.initialSelection = settings->clientTheme() == 1 ? 0 : 1,
		.options = {
			tr::hoursgram_ThemeHoursGram(tr::now),
			tr::hoursgram_ThemeTelegram(tr::now),
		},
		.setter = [=](int value) {
			settings->setClientTheme(value == 0 ? 1 : 0);
		},
	});

	ayu.addSettingToggle({
		.id = u"ayu/materialSwitches"_q,
		.altIds = { u"ayu/newSwitchStyle"_q },
		.title = tr::ayu_MaterialSwitches(),
		.getter = &AyuSettings::materialSwitches,
		.setter = &AyuSettings::setMaterialSwitches,
	});
	ayu.addSettingToggle({
		.id = u"ayu/disableCustomBackgrounds"_q,
		.altIds = { u"ayu/customThemes"_q },
		.title = tr::ayu_DisableCustomBackgrounds(),
		.getter = &AyuSettings::disableCustomBackgrounds,
		.setter = &AyuSettings::setDisableCustomBackgrounds,
	});
	ayu.addSettingToggle({
		.id = u"ayu/hidePremiumStatuses"_q,
		.title = tr::ayu_HidePremiumStatuses(),
		.getter = &AyuSettings::hidePremiumStatuses,
		.setter = &AyuSettings::setHidePremiumStatuses,
	});

	const auto controller = builder.controller();
	builder.addButton({
		.id = u"ayu/monoFont"_q,
		.title = tr::ayu_MonospaceFont(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(
			settings->monoFont().isEmpty()
				? tr::ayu_FontDefault(tr::now)
				: settings->monoFont()),
		.onClick = [=] {
			AyuUi::FontSelectorBox::Show(
				controller,
				[=](const QString &font) {
					AyuSettings::getInstance().setMonoFont(font);
				});
		},
	});

	ayu.addSectionDivider();
}

void AddLocalGift(
		SectionBuilder &builder,
		not_null<Window::SessionController*> controller,
		uint64 userId,
		QString giftId,
		rpl::producer<QString> title,
		int price) {
	auto *settings = &AyuSettings::getInstance();
	auto label = settings->localOwnedGiftsValue(userId) | rpl::map(
		[giftId, price](const QStringList &gifts) {
			if (gifts.contains(giftId)) {
				return tr::hoursgram_GiftOwned(tr::now);
			}
			return QString("%1 · %2 ★")
				.arg(tr::hoursgram_AddGift(tr::now))
				.arg(price);
		});
	const auto id = u"hoursgram/gifts/"_q + giftId;
	builder.addButton({
		.id = id,
		.title = std::move(title),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(label),
		.onClick = [=] {
			if (settings->hasLocalGift(userId, giftId)) {
				controller->showToast(tr::hoursgram_GiftOwned(tr::now));
			} else if (settings->buyLocalGift(userId, giftId, price)) {
				controller->showToast(tr::hoursgram_GiftAdded(tr::now));
			} else {
				controller->showToast(tr::hoursgram_NotEnoughStars(tr::now));
			}
		},
	});
}

void BuildHoursGramStudio(SectionBuilder &builder) {
	const auto controller = builder.controller();
	const auto userId = controller->session().userId().bare;
	auto *settings = &AyuSettings::getInstance();

	builder.addSubsectionTitle(tr::hoursgram_ProfileStudio());
	builder.addButton({
		.id = u"hoursgram/localStars"_q,
		.title = tr::hoursgram_AddLocalStars(),
		.st = &st::settingsButtonNoIcon,
		.label = settings->localStarsValue(userId) | rpl::map([](int stars) {
			return QString("%1 ★").arg(stars);
		}),
		.onClick = [=] {
			if (settings->addLocalStars(userId, LOCAL_STARS_GRANT)) {
				controller->showToast(tr::hoursgram_StarsAdded(tr::now));
			}
		},
	});
	builder.addSubsectionTitle(tr::hoursgram_LocalProfileIdentity());
	builder.addButton({
		.id = u"hoursgram/localNftUsername"_q,
		.title = tr::hoursgram_LocalNftUsername(),
		.st = &st::settingsButtonNoIcon,
		.label = settings->getLocalProfileUsernameValue(userId) | rpl::map(
			[](const QString &username) {
				return username.isEmpty() ? QString() : '@' + username;
			}),
		.onClick = [=] {
			ShowLocalProfileTextBox(
				controller,
				tr::hoursgram_LocalNftUsername(),
				tr::hoursgram_LocalNftUsername(),
				settings->getLocalProfileUsername(userId),
				[=](QString value) {
					settings->setLocalProfileUsername(userId, value);
				});
		},
	});
	builder.addButton({
		.id = u"hoursgram/localProfilePhone"_q,
		.title = tr::hoursgram_LocalProfilePhone(),
		.st = &st::settingsButtonNoIcon,
		.label = settings->getLocalProfilePhoneValue(userId),
		.onClick = [=] {
			ShowLocalProfileTextBox(
				controller,
				tr::hoursgram_LocalProfilePhone(),
				tr::hoursgram_LocalProfilePhone(),
				settings->getLocalProfilePhone(userId),
				[=](QString value) {
					settings->setLocalProfilePhone(userId, value);
				});
		},
	});

	builder.addSubsectionTitle(tr::hoursgram_LocalGiftMarket());
	builder.addButton({
		.id = u"hoursgram/browseTelegramGifts"_q,
		.title = tr::hoursgram_BrowseTelegramGifts(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				const auto peer = controller->session().user();
				box->setTitle(tr::hoursgram_LocalGiftMarket());
				box->setWidth(st::boxWideWidth);
				box->setMaxHeight(st::boxWideWidth * 2);
				box->addButton(tr::lng_close(), [=] {
					box->closeBox();
				});
				box->addRow(object_ptr<Ui::FlatLabel>(
					box,
					tr::hoursgram_LocalOnlyNotice(),
					st::boxDividerLabel));
				box->addRow(object_ptr<Ui::FlatLabel>(
					box,
					tr::hoursgram_LocalGiftPickerHint(),
					st::boxDividerLabel));

				auto gifts = Info::PeerGifts::GiftsStars(
					&controller->session(),
					peer
				) | rpl::map([](
						std::vector<Info::PeerGifts::GiftTypeStars> entries) {
					Ui::GiftsDescriptor result;
					for (const auto &entry : entries) {
						if (!entry.resale) {
							result.list.push_back(entry);
						}
					}
					return result;
				});
				box->addRow(Ui::MakeGiftsList({
					.window = controller,
					.mode = Ui::GiftsListMode::Local,
					.peer = peer,
					.gifts = std::move(gifts),
					.handler = [=](Info::PeerGifts::GiftDescriptor descriptor) {
						const auto gift = std::get_if<Info::PeerGifts::GiftTypeStars>(
							&descriptor);
						if (!gift) {
							return;
						}
						const auto baseId = QString::number(gift->info.id);
						const auto oldId = u"telegram:"_q + baseId;
						if (settings->removeLocalGift(userId, oldId)) {
							controller->showToast(
								tr::hoursgram_GiftRemoved(tr::now));
							return;
						}
						const auto prefix = u"nft:"_q + baseId + ':';
						for (const auto &localGift : settings->localOwnedGifts(userId)) {
							if (localGift.startsWith(prefix)) {
								if (settings->removeLocalGift(userId, localGift)) {
									controller->showToast(
										tr::hoursgram_GiftRemoved(tr::now));
								}
								return;
							}
						}
						ShowLocalNftNumberBox(controller, [=](int number) {
							const auto id = prefix + QString::number(number);
							if (settings->addLocalGift(userId, id)) {
								controller->showToast(
									tr::hoursgram_GiftAdded(tr::now));
							}
						});
					},
					.localSelection = [=](uint64 giftId) {
						const auto id = QString::number(giftId);
						const auto prefix = u"nft:"_q + id + ':';
						for (const auto &localGift : settings->localOwnedGifts(userId)) {
							if (localGift == u"telegram:"_q + id
								|| localGift.startsWith(prefix)) {
								return true;
							}
						}
						return false;
					},
				}), QMargins());
			}));
		},
	});

	for (const auto &gift : LOCAL_GIFT_OFFERS) {
		AddLocalGift(
			builder,
			controller,
			userId,
			gift.id,
			GetLocalGiftTitle(gift.id),
			gift.price);
	}
	builder.addDividerText(tr::hoursgram_LocalOnlyNotice());
	builder.addSkip();
}

void BuildChatFolders(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_ChatFoldersHeader());

	ayu.addSettingToggle({
		.id = u"ayu/hideNotificationCounters"_q,
		.altIds = { u"ayu/tabCounter"_q },
		.title = tr::ayu_HideNotificationCounters(),
		.getter = &AyuSettings::hideNotificationCounters,
		.setter = &AyuSettings::setHideNotificationCounters,
	});
	ayu.addSettingToggle({
		.id = u"ayu/hideAllChatsFolder"_q,
		.altIds = { u"ayu/hideAllChats"_q },
		.title = tr::ayu_HideAllChats(),
		.getter = &AyuSettings::hideAllChatsFolder,
		.setter = &AyuSettings::setHideAllChatsFolder,
	});

	ayu.addSectionDivider();
}

void BuildTrayElements(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_TrayElementsHeader());

	ayu.addSettingToggle({
		.id = u"ayu/showGhostToggleInTray"_q,
		.title = tr::ayu_EnableGhostModeTray(),
		.getter = &AyuSettings::showGhostToggleInTray,
		.setter = &AyuSettings::setShowGhostToggleInTray,
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	ayu.addSettingToggle({
		.id = u"ayu/showStreamerToggleInTray"_q,
		.title = tr::ayu_EnableStreamerModeTray(),
		.getter = &AyuSettings::showStreamerToggleInTray,
		.setter = &AyuSettings::setShowStreamerToggleInTray,
	});
#endif

	ayu.addSectionDivider();
}

void BuildDrawerElements(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_DrawerElementsHeader());

	ayu.addSettingToggle({
		.id = u"ayu/showMyProfileInDrawer"_q,
		.title = tr::lng_menu_my_profile(),
		.getter = &AyuSettings::showMyProfileInDrawer,
		.setter = &AyuSettings::setShowMyProfileInDrawer,
		.icon = { &st::menuIconProfile },
	});

	const auto controller = builder.controller();
	if (controller && HasDrawerBots(controller)) {
		ayu.addSettingToggle({
			.id = u"ayu/showBotsInDrawer"_q,
			.title = tr::lng_filters_type_bots(),
			.getter = &AyuSettings::showBotsInDrawer,
			.setter = &AyuSettings::setShowBotsInDrawer,
			.icon = { &st::menuIconBot },
		});
	}

	ayu.addSettingToggle({
		.id = u"ayu/showNewGroupInDrawer"_q,
		.title = tr::lng_create_group_title(),
		.getter = &AyuSettings::showNewGroupInDrawer,
		.setter = &AyuSettings::setShowNewGroupInDrawer,
		.icon = { &st::menuIconGroups },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showNewChannelInDrawer"_q,
		.title = tr::lng_create_channel_title(),
		.getter = &AyuSettings::showNewChannelInDrawer,
		.setter = &AyuSettings::setShowNewChannelInDrawer,
		.icon = { &st::menuIconChannel },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showContactsInDrawer"_q,
		.title = tr::lng_menu_contacts(),
		.getter = &AyuSettings::showContactsInDrawer,
		.setter = &AyuSettings::setShowContactsInDrawer,
		.icon = { &st::menuIconUserShow },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showCallsInDrawer"_q,
		.title = tr::lng_menu_calls(),
		.getter = &AyuSettings::showCallsInDrawer,
		.setter = &AyuSettings::setShowCallsInDrawer,
		.icon = { &st::menuIconPhone },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showSavedMessagesInDrawer"_q,
		.title = tr::lng_saved_messages(),
		.getter = &AyuSettings::showSavedMessagesInDrawer,
		.setter = &AyuSettings::setShowSavedMessagesInDrawer,
		.icon = { &st::menuIconSavedMessages },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showLReadToggleInDrawer"_q,
		.title = tr::ayu_LReadMessages(),
		.getter = &AyuSettings::showLReadToggleInDrawer,
		.setter = &AyuSettings::setShowLReadToggleInDrawer,
		.icon = { &st::ayuLReadMenuIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showSReadToggleInDrawer"_q,
		.title = tr::ayu_SReadMessages(),
		.getter = &AyuSettings::showSReadToggleInDrawer,
		.setter = &AyuSettings::setShowSReadToggleInDrawer,
		.icon = { &st::ayuSReadMenuIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showNightModeToggleInDrawer"_q,
		.title = tr::lng_menu_night_mode(),
		.getter = &AyuSettings::showNightModeToggleInDrawer,
		.setter = &AyuSettings::setShowNightModeToggleInDrawer,
		.icon = { &st::menuIconNightMode },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showGhostToggleInDrawer"_q,
		.title = tr::ayu_GhostModeToggle(),
		.getter = &AyuSettings::showGhostToggleInDrawer,
		.setter = &AyuSettings::setShowGhostToggleInDrawer,
		.icon = { &st::ayuGhostIcon },
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	ayu.addSettingToggle({
		.id = u"ayu/showStreamerToggleInDrawer"_q,
		.title = tr::ayu_StreamerModeToggle(),
		.getter = &AyuSettings::showStreamerToggleInDrawer,
		.setter = &AyuSettings::setShowStreamerToggleInDrawer,
		.icon = { &st::ayuStreamerModeMenuIcon },
	});
#endif

	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = AyuAppearance::Id(),
	.parentId = AyuMain::Id(),
	.title = &tr::ayu_CategoryAppearance,
	.icon = &st::menuIconPalette,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);

	builder.addSkip();
	BuildAppIcon(builder, ayu);
	BuildAvatarCorners(builder, ayu);
	BuildHoursGramStudio(builder);
	BuildAppearance(builder, ayu);
	BuildChatFolders(builder, ayu);
	BuildTrayElements(builder, ayu);
	BuildDrawerElements(builder, ayu);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> AyuAppearance::title() {
	return tr::ayu_CategoryAppearance();
}

AyuAppearance::AyuAppearance(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AyuAppearance::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AyuAppearanceId() {
	return AyuAppearance::Id();
}

} // namespace Settings
