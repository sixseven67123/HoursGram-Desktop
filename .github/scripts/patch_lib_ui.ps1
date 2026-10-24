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
	$buttons.Insert($anonymousNamespace + 2, '')
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
if (!$buttons.Contains("`tconst auto radius = (height() > 28) ? kSettingsButtonRadius : (height() / 2);")) {
	$beforeRipple = [string[]]@(
		"`tconst auto radius = (height() > 28) ? kSettingsButtonRadius : (height() / 2);",
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
		"`tconst auto radius = (rect.height() > 28) ? kSettingsButtonRadius : (rect.height() / 2);",
		"`tp.save();",
		"`tp.setPen(Qt::NoPen);",
		"`tp.setBrush(color);",
		"`tp.drawRoundedRect(rect, radius, radius);",
		"`tp.restore();"
	))
} elseif (!$buttons.Contains("`tp.drawRoundedRect(rect, radius, radius);")) {
	throw 'SettingsButton background painting is not in the expected form.'
}
Write-Lines $buttonsPath $buttons

Write-Output 'Applied 14px HorusGram SettingsButton radius to lib_ui.'

