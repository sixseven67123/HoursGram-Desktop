param(
	[string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
)

$ErrorActionPreference = 'Stop'

function Read-Lines([string]$Path) {
	$lines = [System.Collections.Generic.List[string]]::new()
	foreach ($line in Get-Content -LiteralPath $Path) {
		[void]$lines.Add($line)
	}
	return ,$lines
}

function Write-Lines([string]$Path, [System.Collections.Generic.List[string]]$Lines) {
	[System.IO.File]::WriteAllLines(
		$Path,
		[string[]]$Lines,
		[System.Text.UTF8Encoding]::new($false))
}

$buttonsPath = Join-Path $WorkspaceRoot 'Telegram/lib_ui/ui/widgets/buttons.cpp'
if (!(Test-Path -LiteralPath $buttonsPath)) {
	throw 'The pinned lib_ui checkout is missing buttons.cpp.'
}

$buttons = Read-Lines $buttonsPath
$anonymousNamespace = $buttons.IndexOf('namespace {')
if ($anonymousNamespace -lt 0) {
	throw 'Could not find the lib_ui anonymous namespace.'
}
if (!$buttons.Contains('constexpr auto kSettingsButtonRadius = 14;')) {
	$buttons.Insert($anonymousNamespace + 1, 'constexpr auto kSettingsButtonRadius = 14;')
}

$eventsInclude = $buttons.IndexOf('#include <QtGui/QtEvents>')
if ($eventsInclude -lt 0) {
	throw 'Could not find the Qt events include in lib_ui buttons.cpp.'
}
if (!$buttons.Contains('#include <QtGui/QPainterPath>')) {
	$buttons.Insert($eventsInclude, '#include <QtGui/QPainterPath>')
}

$paintEvent = $buttons.IndexOf('void SettingsButton::paintEvent(QPaintEvent *e) {')
$ripple = -1
for ($i = $paintEvent + 1; $paintEvent -ge 0 -and $i -lt $buttons.Count; $i++) {
	if ($buttons[$i] -eq "`tpaintRipple(p, 0, 0);") {
		$ripple = $i
		break
	}
	if ($buttons[$i] -eq '}') {
		break
	}
}
if ($paintEvent -lt 0 -or $ripple -lt 0) {
	throw 'Could not find the SettingsButton ripple paint call.'
}
if (!$buttons.Contains("`tconst auto scaled = style::ConvertScale(kSettingsButtonRadius);")) {
	$beforeRipple = [string[]]@(
		"`tconst auto scaled = style::ConvertScale(kSettingsButtonRadius);",
		"`tconst auto radius = (height() > 2 * scaled) ? scaled : (height() / 2);",
		"`tauto clip = QPainterPath();",
		"`tclip.addRoundedRect(rect(), radius, radius);",
		"`tp.save();",
		"`tp.setClipPath(clip);"
	)
	$buttons.InsertRange($ripple, $beforeRipple)
	$ripple += $beforeRipple.Length
	$buttons.InsertRange($ripple + 1, [string[]]@(
		"`tp.restore();"
	))
}

$paintBg = $buttons.IndexOf('void SettingsButton::paintBg(Painter &p, const QRect &rect, bool over) const {')
$fillRect = -1
for ($i = $paintBg + 1; $paintBg -ge 0 -and $i -lt $buttons.Count; $i++) {
	if ($buttons[$i] -eq "`tp.fillRect(rect, over ? _st.textBgOver : _st.textBg);") {
		$fillRect = $i
		break
	}
	if ($buttons[$i] -eq '}') {
		break
	}
}
if ($paintBg -lt 0) {
	throw 'Could not find the SettingsButton background painter.'
}
if ($fillRect -ge 0) {
	$buttons.RemoveAt($fillRect)
	$buttons.InsertRange($fillRect, [string[]]@(
		"`tconst auto color = over ? _st.textBgOver : _st.textBg;",
		"`tconst auto scaled = style::ConvertScale(kSettingsButtonRadius);",
		"`tconst auto radius = (rect.height() > 2 * scaled) ? scaled : (rect.height() / 2);",
		"`tp.save();",
		"`tauto hq = PainterHighQualityEnabler(p);",
		"`tp.setPen(Qt::NoPen);",
		"`tp.setBrush(color);",
		"`tp.drawRoundedRect(this->rect(), radius, radius);",
		"`tp.restore();"
	))
} elseif ($buttons.Contains("`tp.drawRoundedRect(rect, radius, radius);")) {
	$buttons[$buttons.IndexOf("`tp.drawRoundedRect(rect, radius, radius);")] = "`tp.drawRoundedRect(this->rect(), radius, radius);"
} elseif (!$buttons.Contains("`tp.drawRoundedRect(this->rect(), radius, radius);")) {
	throw 'SettingsButton background painting is not in the expected form.'
}
Write-Lines $buttonsPath $buttons

Write-Output 'Applied 14px HorusGram SettingsButton radius to lib_ui.'


function Edit-Text(
		[string]$Path,
		[string]$Skip,
		[hashtable[]]$Replacements) {
	if (!(Test-Path -LiteralPath $Path)) {
		throw "The pinned lib_ui checkout is missing $Path."
	}
	$text = [System.IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
	if ($text.Contains($Skip)) {
		return
	}
	foreach ($replacement in $Replacements) {
		$old = $replacement.Old
		$new = $replacement.New
		$index = $text.IndexOf($old)
		if ($index -lt 0) {
			throw "Could not find the expected fragment in $Path."
		}
		if ($text.IndexOf($old, $index + 1) -ge 0) {
			throw "The expected fragment is ambiguous in $Path."
		}
		$text = $text.Remove($index, $old.Length).Insert($index, $new)
	}
	[System.IO.File]::WriteAllText(
		$Path,
		$text,
		[System.Text.UTF8Encoding]::new($false))
}

# A separate heading family (Montserrat Alternates) for semibold styles.
Edit-Text `
	(Join-Path $WorkspaceRoot 'Telegram/lib_ui/ui/style/style_core_font.h') `
	'void SetCustomSemiboldFont(const QString &font);' `
	@(
		@{
			Old = "void SetCustomFont(const QString &font);`n";
			New = "void SetCustomFont(const QString &font);`nvoid SetCustomSemiboldFont(const QString &font);`n";
		})

Edit-Text `
	(Join-Path $WorkspaceRoot 'Telegram/lib_ui/ui/style/style_core_font.cpp') `
	'CustomSemibold' `
	@(
		@{
			Old = "QString Custom;`n";
			New = "QString Custom;`nQString CustomSemibold;`n";
		},
		@{
			Old = "void SetCustomFont(const QString &font) {`n`tCustom = font;`n}`n";
			New = "void SetCustomFont(const QString &font) {`n`tCustom = font;`n}`n`nvoid SetCustomSemiboldFont(const QString &font) {`n`tCustomSemibold = font;`n}`n";
		},
		@{
			Old = "`t} else if (overriden) {`n`t`tfont.setFamily(family);`n";
			New = "`t} else if (overriden) {`n`t`tfont.setFamily(((flags & FontFlag::Bold) && !CustomSemibold.isEmpty())`n`t`t`t? CustomSemibold`n`t`t`t: family);`n";
		})

# Rounded hover / ripple for every popup and main menu row.
Edit-Text `
	(Join-Path $WorkspaceRoot 'Telegram/lib_ui/ui/widgets/menu/menu_action.cpp') `
	'kHoursGramMenuRadius' `
	@(
		@{
			Old = "#include <QtGui/QtEvents>`n";
			New = "#include <QtGui/QPainterPath>`n#include <QtGui/QtEvents>`n";
		},
		@{
			Old = "namespace Ui::Menu {`nnamespace {`n";
			New = "namespace Ui::Menu {`nnamespace {`n`nconstexpr auto kHoursGramMenuRadius = 10;`nconstexpr auto kHoursGramMenuMargin = 6;`n`n[[nodiscard]] QRect HoursGramMenuRect(int width, int height) {`n`tconst auto margin = style::ConvertScale(kHoursGramMenuMargin);`n`tconst auto skip = (margin < (width / 8)) ? margin : (width / 8);`n`treturn QRect(skip, 0, width - 2 * skip, height);`n}`n`n[[nodiscard]] int HoursGramMenuRadius(int height) {`n`tconst auto radius = style::ConvertScale(kHoursGramMenuRadius);`n`treturn (radius < (height / 2)) ? radius : (height / 2);`n}`n";
		},
		@{
			Old = "void Action::paintBackground(QPainter &p, bool selected) {`n`tif (selected && _st.itemBgOver->c.alpha() < 255) {`n`t`tp.fillRect(0, 0, width(), _height, _st.itemBg);`n`t}`n`tp.fillRect(`n`t`tQRect(0, 0, width(), _height),`n`t`tselected ? _st.itemBgOver : _st.itemBg);`n}`n";
			New = "void Action::paintBackground(QPainter &p, bool selected) {`n`tp.fillRect(0, 0, width(), _height, _st.itemBg);`n`tif (!selected) {`n`t`treturn;`n`t}`n`tauto hq = PainterHighQualityEnabler(p);`n`tconst auto radius = HoursGramMenuRadius(_height);`n`tp.setPen(Qt::NoPen);`n`tp.setBrush(_st.itemBgOver);`n`tp.drawRoundedRect(HoursGramMenuRect(width(), _height), radius, radius);`n}`n";
		},
		@{
			Old = "`tpaintBackground(p, selected);`n`tif (enabled) {`n`t`tRippleButton::paintRipple(p, 0, 0);`n`t}`n";
			New = "`tpaintBackground(p, selected);`n`tif (enabled) {`n`t`tconst auto radius = HoursGramMenuRadius(_height);`n`t`tauto clip = QPainterPath();`n`t`tclip.addRoundedRect(`n`t`t`tHoursGramMenuRect(width(), _height),`n`t`t`tradius,`n`t`t`tradius);`n`t`tp.save();`n`t`tp.setClipPath(clip);`n`t`tRippleButton::paintRipple(p, 0, 0);`n`t`tp.restore();`n`t}`n";
		})

# Rounder boxes and popup menus.
Edit-Text `
	(Join-Path $WorkspaceRoot 'Telegram/lib_ui/ui/layers/layers.style') `
	'boxRadius: 14px;' `
	@(
		@{
			Old = "boxRadius: 6px;`n";
			New = "boxRadius: 14px;`n";
		})

Edit-Text `
	(Join-Path $WorkspaceRoot 'Telegram/lib_ui/ui/widgets/widgets.style') `
	'radius: 12px;' `
	@(
		@{
			Old = "`tradius: 6px;`n`tduration: 150;`n";
			New = "`tradius: 12px;`n`tduration: 150;`n";
		})

Write-Output 'Applied HoursGram fonts and rounded menu rows to lib_ui.'
