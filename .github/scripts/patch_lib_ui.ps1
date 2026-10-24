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

$stylePath = Join-Path $WorkspaceRoot 'Telegram/lib_ui/ui/widgets/widgets.style'
$buttonsPath = Join-Path $WorkspaceRoot 'Telegram/lib_ui/ui/widgets/buttons.cpp'
if (!(Test-Path -LiteralPath $stylePath) -or !(Test-Path -LiteralPath $buttonsPath)) {
	throw 'The pinned lib_ui checkout is missing the SettingsButton sources.'
}

$style = Read-Lines $stylePath
$settingsIndex = $style.IndexOf('SettingsButton {')
$defaultIndex = $style.IndexOf('defaultSettingsButton: SettingsButton {')
if ($settingsIndex -lt 0 -or $defaultIndex -le $settingsIndex) {
	throw 'Could not find the SettingsButton style declarations in lib_ui.'
}
$iconLeftIndex = -1
for ($i = $settingsIndex + 1; $i -lt $defaultIndex; $i++) {
	if ($style[$i] -eq "`ticonLeft: pixels;") {
		$iconLeftIndex = $i
		break
	}
}
if ($iconLeftIndex -lt 0) {
	throw 'Could not find the SettingsButton iconLeft field.'
}
if ($style[$iconLeftIndex + 1] -ne "`tradius: pixels;") {
	$style.Insert($iconLeftIndex + 1, "`tradius: pixels;")
}

$defaultIndex = $style.IndexOf('defaultSettingsButton: SettingsButton {')
$rightLabelIndex = -1
for ($i = $defaultIndex + 1; $i -lt $style.Count; $i++) {
	if ($style[$i] -eq "`trightLabel: defaultSettingsRightLabel;") {
		$rightLabelIndex = $i
		break
	}
	if ($style[$i] -eq '}') {
		break
	}
}
if ($rightLabelIndex -lt 0) {
	throw 'Could not find the default SettingsButton style.'
}
if ($style[$rightLabelIndex + 1] -ne "`tradius: 0px;") {
	$style.Insert($rightLabelIndex + 1, "`tradius: 0px;")
}
Write-Lines $stylePath $style

$buttons = Read-Lines $buttonsPath
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
if (!$buttons.Contains("`tconst auto rounded = (_st.radius > 0);")) {
	$beforeRipple = [string[]]@(
		"`tconst auto rounded = (_st.radius > 0);",
		"`tif (rounded) {",
		"`t`tauto clip = QPainterPath();",
		"`t`tclip.addRoundedRect(rect(), _st.radius, _st.radius);",
		"`t`tp.save();",
		"`t`tp.setClipPath(clip);",
		"`t}"
	)
	$buttons.InsertRange($ripple, $beforeRipple)
	$ripple += $beforeRipple.Length
	$buttons.InsertRange($ripple + 1, [string[]]@(
		"`tif (rounded) {",
		"`t`tp.restore();",
		"`t}"
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
		"`tif (_st.radius <= 0) {",
		"`t`tp.fillRect(rect, color);",
		"`t`treturn;",
		"`t}",
		"`tp.save();",
		"`tp.setPen(Qt::NoPen);",
		"`tp.setBrush(color);",
		"`tp.drawRoundedRect(rect, _st.radius, _st.radius);",
		"`tp.restore();"
	))
} elseif (!$buttons.Contains("`tp.drawRoundedRect(rect, _st.radius, _st.radius);")) {
	throw 'SettingsButton background painting is not in the expected form.'
}
Write-Lines $buttonsPath $buttons

Write-Output 'Applied rounded SettingsButton painting to lib_ui.'

