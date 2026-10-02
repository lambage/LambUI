local palette = {
    paper = 0xF0F3F2FF, ink = 0x1D2D2AFF, muted = 0x66756FFF,
    line = 0xD6DEDAFF, forest = 0x244D40FF, mint = 0xB9E5CDFF,
    white = 0xFFFFFFFF, rose = 0xB54862FF
}

App = { selected = 1, running = false, elapsed = 0, targets = {} }
App.routes = {
    { name = "North ridge", region = "ALPINE / SECTOR 01", distance = "12.4 km", duration = "4h 20m", gain = "+860 m",
      description = "Follow the ridgeline above the lake. The final ascent opens onto the northern observation point.",
      notes = "Meet at base camp at 07:30.\nPack warm layers and two litres of water." },
    { name = "Lake circuit", region = "WETLAND / SECTOR 02", distance = "8.6 km", duration = "2h 45m", gain = "+210 m",
      description = "Survey the shoreline and cross the eastern footbridge. Record water levels at all three stations.",
      notes = "Take sample bottles and the field notebook.\nReturn before the afternoon weather change." },
    { name = "Eastern traverse", region = "FOREST / SECTOR 03", distance = "16.2 km", duration = "5h 10m", gain = "+640 m",
      description = "Cross the forest plateau to the east shelter. Inspect the trail markers along the old boundary.",
      notes = "Check radio batteries.\nLeave a route copy at the ranger station." }
}

local function frame(kind, name, parent)
    return UI.CreateFrame(kind, name, parent or UI.Root)
end
local function place(widget, parent, x, y, width, height)
    widget:ClearPoints()
    widget:SetSize(width, height)
    widget:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y)
end
local function label(parent, name, text, color, heading)
    local widget = parent:CreateFontString(name)
    widget:SetMouseEnabled(false)
    if heading then widget:SetFont(Host.headingFont) end
    widget:SetText(text)
    widget:SetColor(color or palette.ink)
    return widget
end
local function button(name, parent, text, action, accent)
    local widget = frame("Button", name, parent)
    widget:SetButtonColors(accent and palette.forest or 0xE0E8E3FF,
        accent and 0x356853FF or 0xCBDDD2FF, accent and 0x17392DFF or 0xB4CCBDFF)
    local title = label(widget, name .. "Label", text, accent and palette.white or palette.ink)
    title:SetPoint("CENTER", widget, "CENTER", 0, 0)
    widget:SetScript("OnClick", action)
    App.targets[name] = widget
    return widget, title
end

local background = frame("Frame", "Background")
background:SetAllPoints(UI.Root)
background:SetBackgroundColor(palette.paper)
background:SetMouseEnabled(false)
local header = frame("Frame", "Header")
header:SetBackgroundColor(palette.ink)
local brand = label(header, "Brand", "Fieldwork", palette.white, true)
local subtitle = label(header, "Subtitle", "EXPEDITION PLANNER", palette.mint)
App.viewport = frame("ScrollContainer", "Workspace")
local content = App.viewport:GetContent()
local routesTitle = label(content, "RoutesTitle", "Saved expeditions", palette.ink)
local routeButtons, routeNames, routeMeta = {}, {}, {}
local detail = frame("Frame", "RouteDetail", content)
local region = label(detail, "Region", "", palette.muted)
local title = label(detail, "RouteTitle", "", palette.ink, true)
local metrics = label(detail, "Metrics", "", palette.forest)
local description = label(detail, "Description", "", palette.muted)
description:SetWordWrap(true)
local map = detail:CreateTexture()
map:SetTexture(Host.map)
map:SetMouseEnabled(false)
local mapCaption = label(detail, "MapCaption", "FIELD SURVEY  /  1:25,000", palette.forest)
local north = label(detail, "North", "N", palette.ink)
local markers = {}
for index = 1, 5 do
    local marker = frame("Frame", "Waypoint" .. index, detail)
    marker:SetBackgroundColor(palette.rose)
    marker:SetMouseEnabled(false)
    markers[index] = marker
end
local notesTitle = label(detail, "NotesTitle", "Field briefing", palette.ink)
App.notes = frame("EditBox", "Briefing", detail)
App.notes:SetMultiline(true)
App.notes:SetWordWrap(true)
App.notes:SetBackgroundColor(palette.ink)
App.notes:SetTooltip("Briefing for the selected expedition")
App.targets.notes = App.notes
local save = button("save", detail, "Save briefing", function()
    local text = App.notes:GetText()
    if not text:find("%S") then App.status:SetText("The briefing cannot be empty."); return end
    App.routes[App.selected].notes = text
    App.status:SetText("Briefing saved for " .. App.routes[App.selected].name .. ".")
end)
App.ready = frame("CheckBox", "Readiness", detail)
App.ready:SetText("")
local readyLabel = label(detail, "ReadinessLabel", "Equipment checked", palette.ink)
App.ready:SetTooltip("Confirm equipment before starting the expedition")
App.targets.ready = App.ready
App.progress = frame("ProgressBar", "Progress", detail)
App.progress:SetMinMaxValues(0, 100)
App.progress:SetProgressColors(palette.line, 0x459475FF)
App.progress:SetMouseEnabled(false)
local progressLabel = label(detail, "ProgressLabel", "Awaiting departure", palette.muted)
local launch, launchLabel
launch, launchLabel = button("launch", detail, "Start expedition", function()
    if App.running then
        App.running = false
        launchLabel:SetText("Resume expedition")
        App.status:SetText("Expedition paused.")
    elseif not App.ready:IsChecked() then
        App.status:SetText("Complete the readiness check first.")
    else
        if App.progress:GetValue() >= 100 then App.elapsed = 0; App.progress:SetValue(0) end
        App.running = true
        launchLabel:SetText("Pause expedition")
        App.status:SetText("Expedition in progress.")
    end
end, true)
local reset = button("reset", detail, "Reset", function()
    App.running = false
    App.elapsed = 0
    App.progress:SetValue(0)
    App.ready:SetChecked(false)
    launchLabel:SetText("Start expedition")
    progressLabel:SetText("Awaiting departure")
    App.status:SetText("Expedition reset.")
end)
App.ready:SetScript("OnValueChanged", function()
    if not App.ready:IsChecked() and App.running then
        App.running = false
        launchLabel:SetText("Resume expedition")
        App.status:SetText("Paused: equipment needs checking.")
    end
end)
local footer = frame("Frame", "Footer")
footer:SetBackgroundColor(palette.white)
App.status = label(footer, "Status", "Choose an expedition and prepare your briefing.", palette.muted)
App.status:SetWordWrap(true)
App.notes:SetScript("OnTextChanged", function()
    App.status:SetText("Unsaved briefing.")
end)

function App.Select(index)
    App.selected = index
    App.running = false
    App.elapsed = 0
    App.progress:SetValue(0)
    App.ready:SetChecked(false)
    local route = App.routes[index]
    region:SetText(route.region)
    title:SetText(route.name)
    metrics:SetText(route.distance .. "    /    " .. route.duration .. "    /    " .. route.gain)
    description:SetText(route.description)
    App.notes:SetText(route.notes)
    launchLabel:SetText("Start expedition")
    progressLabel:SetText("Awaiting departure")
    App.status:SetText("Planning " .. route.name .. ".")
    for position, widget in ipairs(routeButtons) do
        widget:SetButtonColors(position == index and palette.forest or palette.white,
            position == index and 0x356853FF or 0xDFEAE3FF, 0x8BAF9BFF)
        routeNames[position]:SetColor(position == index and palette.white or palette.ink)
        routeMeta[position]:SetColor(position == index and palette.mint or palette.muted)
    end
    if App.width then App.Resize(App.width, App.height) end
end

for index, route in ipairs(App.routes) do
    local widget = frame("Button", "Route" .. index, content)
    widget:SetTooltip(route.description)
    widget:SetScript("OnClick", function() App.Select(index) end)
    routeButtons[index] = widget
    routeNames[index] = label(widget, "RouteName" .. index, route.name)
    routeMeta[index] = label(widget, "RouteMeta" .. index, route.distance .. "  /  " .. route.duration, palette.muted)
    App.targets["route" .. index] = widget
end

local windowLayer = frame("Frame", "FloatingWindows")
windowLayer:SetMouseEnabled(false)
App.fieldTime, App.fieldPaused = 0, false
App.canvasWindow = frame("Window", "FieldPreview", windowLayer)
App.canvasWindow:SetTitle("Distortion field")
App.canvasWindow:SetSizeLimits(300, 220, 1600, 1200)
local canvasContent = App.canvasWindow:GetContent()
canvasContent:SetBackgroundColor(palette.ink)
App.canvas = frame("Canvas", "FieldCanvas", canvasContent)
App.canvas:SetPoint("TOPLEFT", canvasContent, "TOPLEFT", 8, 8)
App.canvas:SetPoint("BOTTOMRIGHT", canvasContent, "BOTTOMRIGHT", -8, -94)
App.canvas:SetRenderCallback(function(x, y, width, height)
    Host.DrawField(App.fieldTime, App.distortion:GetValue())
end)
App.targets.canvas = App.canvas
local strengthLabel = label(canvasContent, "StrengthLabel", "Distortion", palette.white)
strengthLabel:SetPoint("TOPLEFT", canvasContent, "BOTTOMLEFT", 10, -82)
App.distortion = frame("StatusBar", "Distortion", canvasContent)
App.distortion:SetMinMaxValues(0, 2)
App.distortion:SetValue(0.8)
App.distortion:SetPoint("TOPLEFT", canvasContent, "BOTTOMLEFT", 106, -82)
App.distortion:SetPoint("BOTTOMRIGHT", canvasContent, "BOTTOMRIGHT", -10, -60)
App.distortion:SetTooltip("Field distortion strength")
App.targets.distortion = App.distortion
local pause, pauseLabel
pause, pauseLabel = button("pauseField", canvasContent, "Pause", function()
    App.fieldPaused = not App.fieldPaused
    pauseLabel:SetText(App.fieldPaused and "Resume" or "Pause")
end)
pause:SetSize(92, 32)
pause:SetPoint("BOTTOMLEFT", canvasContent, "BOTTOMLEFT", 10, -10)
local resetField = button("resetField", canvasContent, "Reset phase", function() App.fieldTime = 0 end)
resetField:SetSize(120, 32)
resetField:SetPoint("BOTTOMRIGHT", canvasContent, "BOTTOMRIGHT", -10, -10)

App.notesWindow = frame("Window", "Scratchpad", windowLayer)
App.notesWindow:SetTitle("Scratchpad")
App.notesWindow:SetSizeLimits(260, 180, 1200, 1000)
local scratchContent = App.notesWindow:GetContent()
scratchContent:SetBackgroundColor(0xEFE4E7FF)
local scratch = frame("EditBox", "ScratchText", scratchContent)
scratch:SetMultiline(true)
scratch:SetWordWrap(true)
scratch:SetBackgroundColor(0x402D36FF)
scratch:SetText("Field observations\n\nWind from the northwest.\nVisibility improving along the ridge.")
scratch:SetPoint("TOPLEFT", scratchContent, "TOPLEFT", 10, 10)
scratch:SetPoint("BOTTOMRIGHT", scratchContent, "BOTTOMRIGHT", -10, -54)
App.targets.scratch = scratch
local closeNotes = button("closeNotes", scratchContent, "Close", function() App.notesWindow:Close() end)
closeNotes:SetSize(90, 30)
closeNotes:SetPoint("BOTTOMRIGHT", scratchContent, "BOTTOMRIGHT", -10, -10)
App.canvasWindow:SetVisible(false)
App.notesWindow:SetVisible(false)

function App.ArrangeWindows()
    local wide = App.width >= 760
    local areaHeight = App.height - 130
    App.canvasWindow:SetBounds(wide and 60 or 8, wide and 24 or 12,
        math.min(560, App.width - 28), math.min(390, areaHeight - 24))
    App.notesWindow:SetBounds(wide and math.min(460, App.width - 344) or App.width - math.min(330, App.width - 48) - 6, wide and 144 or 68,
        math.min(330, App.width - 48), math.min(280, areaHeight - 88))
end
local showCanvas = button("showCanvas", header, "Canvas", function()
    App.canvasWindow:SetVisible(true)
    App.canvasWindow:Restore()
    App.canvasWindow:BringToFront()
end)
local showNotes = button("showNotes", header, "Notes", function()
    App.notesWindow:SetVisible(true)
    App.notesWindow:Restore()
    App.notesWindow:BringToFront()
end)
local arrange = button("arrangeWindows", header, "Arrange", function()
    App.ArrangeWindows()
    App.canvasWindow:SetVisible(true)
    App.notesWindow:SetVisible(true)
    App.notesWindow:BringToFront()
end)
showCanvas:SetTooltip("Open distortion preview")
showNotes:SetTooltip("Open scratchpad")
arrange:SetTooltip("Open and arrange floating windows")

function App.Resize(width, height)
    local resized = App.width ~= width or App.height ~= height
    App.width, App.height = width, height
    local compact = width < 760
    place(header, UI.Root, 0, 0, width, 76)
    place(brand, header, 24, 9, 200, 34)
    place(subtitle, header, 26, 47, 250, 22)
    place(showCanvas, header, width - 172, 8, 76, 30)
    place(showNotes, header, width - 88, 8, 70, 30)
    place(arrange, header, width - 100, 44, 82, 26)
    place(windowLayer, UI.Root, 0, 76, width, height - 130)
    if resized then App.ArrangeWindows() end
    place(footer, UI.Root, 0, height - 54, width, 54)
    place(App.status, footer, 24, 8, width - 48, 40)
    place(App.viewport, UI.Root, 0, 76, width, height - 130)
    local margin = compact and 18 or 26
    local leftWidth = compact and width - margin * 2 - 12 or 230
    place(routesTitle, content, margin, 20, leftWidth, 24)
    for index, widget in ipairs(routeButtons) do
        local rowHeight = compact and 62 or 80
        place(widget, content, margin, 56 + (index - 1) * (rowHeight + 10), leftWidth, rowHeight)
        place(routeNames[index], widget, 14, 10, leftWidth - 28, 24)
        place(routeMeta[index], widget, 14, compact and 34 or 43, leftWidth - 28, 22)
    end
    local detailX = compact and margin or 286
    local detailY = compact and 288 or 20
    local detailWidth = width - detailX - margin - 12
    place(detail, content, detailX, detailY, detailWidth, 744)
    place(region, detail, 0, 0, detailWidth, 22)
    place(title, detail, 0, 30, detailWidth, 40)
    place(metrics, detail, 0, 77, detailWidth, 24)
    place(map, detail, 0, 115, detailWidth, compact and 180 or 190)
    local mapHeight = compact and 180 or 190
    place(north, detail, detailWidth - 26, 124, 20, 24)
    for index, marker in ipairs(markers) do
        local fraction = index / 6
        local rise = 0.53 + math.sin(fraction * 5 + App.selected) * 0.24
        place(marker, detail, detailWidth * fraction - 4, 115 + mapHeight * rise, 8, 8)
    end
    local below = 115 + mapHeight
    place(mapCaption, detail, 0, below + 8, detailWidth, 22)
    local extra = compact and 42 or 0
    place(description, detail, 0, below + 40, detailWidth, 58 + extra)
    place(notesTitle, detail, 0, below + 108 + extra, detailWidth, 24)
    place(App.notes, detail, 0, below + 140 + extra, detailWidth, 98)
    place(save, detail, 0, below + 248 + extra, 142, 34)
    if compact then
        place(App.ready, detail, 0, below + 302 + extra, detailWidth, 30)
        place(readyLabel, detail, 28, below + 306 + extra, detailWidth - 28, 24)
        place(progressLabel, detail, 0, below + 344 + extra, detailWidth, 22)
        place(App.progress, detail, 0, below + 376 + extra, detailWidth, 7)
        place(launch, detail, 0, below + 401 + extra, math.min(204, detailWidth - 104), 40)
        place(reset, detail, detailWidth - 86, below + 401 + extra, 86, 40)
        App.viewport:SetContentSize(width - 12, detailY + below + 471 + extra)
    else
        place(App.ready, content, margin, 350, leftWidth, 30)
        place(readyLabel, content, margin + 28, 354, leftWidth - 28, 24)
        place(progressLabel, content, margin, 400, leftWidth, 22)
        place(App.progress, content, margin, 432, leftWidth, 7)
        place(launch, content, margin, 464, leftWidth, 40)
        place(reset, content, margin, 516, leftWidth, 34)
        App.viewport:SetContentSize(width - 12, detailY + below + 310)
    end
end

function App.Reveal(name)
    if name == "showCanvas" or name == "showNotes" or name == "arrangeWindows"
        or name == "pauseField" or name == "resetField" or name == "distortion"
        or name == "closeNotes" or name == "scratch" or name == "canvas" then return end
    local _, targetY, _, targetHeight = App.targets[name]:GetRect()
    local _, viewportY, _, viewportHeight = App.viewport:GetRect()
    if targetY + targetHeight > viewportY + viewportHeight then
        App.viewport:SetScrollOffset(0, targetY + targetHeight - viewportY - viewportHeight + 24)
    end
end

function App.Update(delta)
    if not App.fieldPaused and App.canvasWindow:IsVisible() and App.canvasWindow:GetWindowState() ~= "Minimized" then
        App.fieldTime = (App.fieldTime + delta) % 10000
    end
    if not App.running then return end
    App.elapsed = math.min(30, App.elapsed + delta)
    local progress = App.elapsed / 30 * 100
    App.progress:SetValue(progress)
    progressLabel:SetText(string.format("Survey progress  /  %d%%", math.floor(progress)))
    if progress >= 100 then
        App.running = false
        launchLabel:SetText("Start again")
        App.status:SetText("Survey complete. Expedition log is ready.")
    end
end

App.notes:SetText(App.routes[1].notes)
App.Select(1)