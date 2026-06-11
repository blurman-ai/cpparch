# Local Complexity Drift Examples

Concrete before/after examples from existing functions only.

## Manual Review Summary

- Reviewed examples: `6`
- Clear true positives: `6`
- Clear false positives: `0`
- Borderline/actionability caveat: `2` UI settings/render handlers are structurally noisy but still real in-function complexity growth.

| Example | Verdict | Reason |
|---------|---------|--------|
| 1 | TP | Existing event handler gets a large nested undo-command state block. |
| 2 | TP | Existing injection hook grows async dispatch, tree walk, mutex-protected scan state, and recovery branches. |
| 3 | TP | Existing command processor grows notification/email parsing branches inside the fallback command path. |
| 4 | TP | Existing settings renderer grows climate-preset UI, tooltips, branching, and feature navigation. |
| 5 | TP | Existing upscaling settings renderer grows availability gating, backend diagnostics, loops, and UI conditionals. |
| 6 | TP | Existing message hook grows init/backoff/discovery state machine inside the dispatch function. |

## Example 1: fernandotonon_QtMeshEditor

- Commit: `97676c6944e3`
- File: `src/TransformOperator.cpp`
- Function: `TransformOperator::mouseReleaseEvent`
- Score: `11 -> 136` (`+125`)
- Branch delta: `79`, nesting delta: `4`, deep lines delta: `37`
- Manual verdict: `TP`
- Why: existing mouse-release handler absorbed undo command construction for translate/rotate/scale paths, with nested loops and state branches. This is real in-function complexity growth, not vendor/test/generated noise.

### Diff

```diff
--- before
+++ after
@@ -1,6 +1,85 @@
 {
     if((SelectionSet::getSingleton()->hasNodes()||SelectionSet::getSingleton()->hasEntities()) && (e->button() == Qt::LeftButton))
+    {
+        // Push undo command if a transform was performed on scene nodes
+        if (SelectionSet::getSingleton()->hasNodes() && !mUndoStartPositions.isEmpty())
+        {
+            auto nodes = SelectionSet::getSingleton()->getNodesSelectionList();
+            bool changed = false;
+
+            if (mTransformState == TS_TRANSLATE)
+            {
+                // Compute total delta from start positions
+                Ogre::Vector3 totalDelta = Ogre::Vector3::ZERO;
+                for (int i = 0; i < nodes.size() && i < mUndoStartPositions.size(); ++i)
+                {
+                    Ogre::Vector3 delta = nodes[i]->getPosition() - mUndoStartPositions[i];
+                    if (!delta.isZeroLength())
+                    {
+                        totalDelta = delta;
+                        changed = true;
+                    }
+                }
+                if (changed)
+                {
+                    // Revert to start, then push command (which will redo)
+                    for (int i = 0; i < nodes.size() && i < mUndoStartPositions.size(); ++i)
+                        nodes[i]->setPosition(mUndoStartPositions[i]);
+                    UndoManager::getSingleton()->push(new TranslateCommand(nodes, totalDelta));
+                }
+            }
+            else if (mTransformState == TS_ROTATE)
+            {
... snippet trimmed ...
+                        nodes[i]->setOrientation(mUndoStartOrientations[i]);
+                    }
+                    UndoManager::getSingleton()->push(
+                        new RotateCommand(nodes, totalRotation, m_pTransformNode->getPosition()));
+                }
+            }
+            else if (mTransformState == TS_SCALE)
+            {
+                Ogre::Vector3 totalScale = Ogre::Vector3::UNIT_SCALE;
+                for (int i = 0; i < nodes.size() && i < mUndoStartScales.size(); ++i)
+                {
+                    if (nodes[i]->getScale() != mUndoStartScales[i])
+                    {
+                        totalScale = nodes[i]->getScale() / mUndoStartScales[i];
+                        changed = true;
+                    }
+                }
+                if (changed)
+                {
+                    for (int i = 0; i < nodes.size() && i < mUndoStartScales.size(); ++i)
+                        nodes[i]->setScale(mUndoStartScales[i]);
+                    UndoManager::getSingleton()->push(new ScaleCommand(nodes, totalScale));
+                }
+            }
+        }
+
+        mUndoStartPositions.clear();
+        mUndoStartOrientations.clear();
+        mUndoStartScales.clear();
         mStartPoint = Ogre::Vector3::ZERO;
+        mScaleStartDistance = 0.0f;
+    }
 
     if(m_pSelectionBox->isVisible())
     {
```

### Before

```cpp
{
    if((SelectionSet::getSingleton()->hasNodes()||SelectionSet::getSingleton()->hasEntities()) && (e->button() == Qt::LeftButton))
        mStartPoint = Ogre::Vector3::ZERO;

    if(m_pSelectionBox->isVisible())
    {
        // Perform a box selection
        SelectionMode   selectionMode = NEW_SELECT;
        if(e->modifiers().testFlag(Qt::ControlModifier))
            selectionMode = ADD_SELECT;
        else if(e->modifiers().testFlag(Qt::ShiftModifier))
            selectionMode = DEL_SELECT;

        performBoxSelection(mScreenStart, e->pos(), selectionMode);

        mScreenStart = QPoint(invalidPosition);

        m_pSelectionBox->clear();
        m_pSelectionBox->setVisible(false);
    }
}
```

### After

```cpp
{
    if((SelectionSet::getSingleton()->hasNodes()||SelectionSet::getSingleton()->hasEntities()) && (e->button() == Qt::LeftButton))
    {
        // Push undo command if a transform was performed on scene nodes
        if (SelectionSet::getSingleton()->hasNodes() && !mUndoStartPositions.isEmpty())
        {
            auto nodes = SelectionSet::getSingleton()->getNodesSelectionList();
            bool changed = false;

            if (mTransformState == TS_TRANSLATE)
            {
                // Compute total delta from start positions
                Ogre::Vector3 totalDelta = Ogre::Vector3::ZERO;
                for (int i = 0; i < nodes.size() && i < mUndoStartPositions.size(); ++i)
                {
                    Ogre::Vector3 delta = nodes[i]->getPosition() - mUndoStartPositions[i];
                    if (!delta.isZeroLength())
                    {
                        totalDelta = delta;
                        changed = true;
                    }
                }
                if (changed)
                {
                    // Revert to start, then push command (which will redo)
                    for (int i = 0; i < nodes.size() && i < mUndoStartPositions.size(); ++i)
                        nodes[i]->setPosition(mUndoStartPositions[i]);
                    UndoManager::getSingleton()->push(new TranslateCommand(nodes, totalDelta));
                }
            }
            else if (mTransformState == TS_ROTATE)
            {
                for (int i = 0; i < nodes.size() && i < mUndoStartOrientations.size(); ++i)
                {
                    if (nodes[i]->getOrientation() != mUndoStartOrientations[i])
... snippet trimmed ...
                    }
                }
                if (changed)
                {
                    for (int i = 0; i < nodes.size() && i < mUndoStartScales.size(); ++i)
                        nodes[i]->setScale(mUndoStartScales[i]);
                    UndoManager::getSingleton()->push(new ScaleCommand(nodes, totalScale));
                }
            }
        }

        mUndoStartPositions.clear();
        mUndoStartOrientations.clear();
        mUndoStartScales.clear();
        mStartPoint = Ogre::Vector3::ZERO;
        mScaleStartDistance = 0.0f;
    }

    if(m_pSelectionBox->isVisible())
    {
        // Perform a box selection
        SelectionMode   selectionMode = NEW_SELECT;
        if(e->modifiers().testFlag(Qt::ControlModifier))
            selectionMode = ADD_SELECT;
        else if(e->modifiers().testFlag(Qt::ShiftModifier))
            selectionMode = DEL_SELECT;

        performBoxSelection(mScreenStart, e->pos(), selectionMode);

        mScreenStart = QPoint(invalidPosition);

        m_pSelectionBox->clear();
        m_pSelectionBox->setVisible(false);
    }
}
```

## Example 2: ramensoftware_windhawk-mods

- Commit: `cdca30ff8a2b`
- File: `mods/taskbar-content-presenter-injector.wh.cpp`
- Function: `InjectForElement`
- Score: `6 -> 108` (`+102`)
- Branch delta: `56`, nesting delta: `6`, deep lines delta: `34`
- Manual verdict: `TP`
- Why: existing hook changed from a simple local scan + async schedule call into an async lambda with weak references, visual-tree traversal, mutex-protected state cleanup, and duplicate-scan suppression. This is exactly the kind of local complexity drift the prototype should surface.

### Diff

```diff
--- before
+++ after
@@ -1,8 +1,61 @@
 void InjectForElement(void* pThis) {
+    static thread_local bool t_scanPending = false;
+    if (t_scanPending) return;
+
     try {
         if (auto elem = GetFrameworkElementFromNative(pThis)) {
-            ScanAndInjectRecursive(elem); // Fast local injection
-            ScheduleScanAsync(elem);      // Will silently return if monitor is already scanned
+            auto dispatcher = elem.Dispatcher();
+            if (!dispatcher) return;
+
+            t_scanPending = true;
+            auto weakElem = winrt::make_weak(elem);
+
+            dispatcher.RunAsync(
+                winrt::Windows::UI::Core::CoreDispatcherPriority::Low,
+                [weakElem]() {
+                    t_scanPending = false;
+                    auto elem = weakElem.get();
+                    if (!elem) return;
+
+                    ScanAndInjectRecursive(elem);
+
+                    FrameworkElement current = elem;
+                    while (current) {
+                        if (winrt::get_class_name(current) == c_RootFrameName) {
+                            void* frameAbi = winrt::get_abi(current);
+                            bool needsScan = false;
+                            
+                            {
+                                std::lock_guard<std::mutex> lock(g_pendingMutex);
+                                bool found = false;
+                                auto it = g_scannedFrames.begin();
+                                while (it != g_scannedFrames.end()) {
+                                    auto existing = it->get();
+                                    if (!existing) {
+                                        it = g_scannedFrames.erase(it);
+                                    } else if (winrt::get_abi(existing) == frameAbi) {
+                                        found = true;
+                                        ++it;
+                                    } else {
+                                        ++it;
+                                    }
+                                }
+                                if (!found) {
+                                    g_scannedFrames.push_back(winrt::make_weak(current));
+                                    needsScan = true;
+                                }
+                            }
+                            
+                            if (needsScan) ScanAndInjectRecursive(current);
+                            break;
+                        }
+                        
+                        auto parent = Media::VisualTreeHelper::GetParent(current);
+                        current = parent ? parent.try_as<FrameworkElement>() : nullptr;
+                    }
+                });
         }
-    } catch (...) {}
+    } catch (...) {
+        t_scanPending = false;
+    }
 }
```

### Before

```cpp
void InjectForElement(void* pThis) {
    try {
        if (auto elem = GetFrameworkElementFromNative(pThis)) {
            ScanAndInjectRecursive(elem); // Fast local injection
            ScheduleScanAsync(elem);      // Will silently return if monitor is already scanned
        }
    } catch (...) {}
}
```

### After

```cpp
void InjectForElement(void* pThis) {
    static thread_local bool t_scanPending = false;
    if (t_scanPending) return;

    try {
        if (auto elem = GetFrameworkElementFromNative(pThis)) {
            auto dispatcher = elem.Dispatcher();
            if (!dispatcher) return;

            t_scanPending = true;
            auto weakElem = winrt::make_weak(elem);

            dispatcher.RunAsync(
                winrt::Windows::UI::Core::CoreDispatcherPriority::Low,
                [weakElem]() {
                    t_scanPending = false;
                    auto elem = weakElem.get();
                    if (!elem) return;

                    ScanAndInjectRecursive(elem);

                    FrameworkElement current = elem;
                    while (current) {
                        if (winrt::get_class_name(current) == c_RootFrameName) {
                            void* frameAbi = winrt::get_abi(current);
                            bool needsScan = false;
                            
                            {
                                std::lock_guard<std::mutex> lock(g_pendingMutex);
                                bool found = false;
                                auto it = g_scannedFrames.begin();
                                while (it != g_scannedFrames.end()) {
                                    auto existing = it->get();
                                    if (!existing) {
                                        it = g_scannedFrames.erase(it);
                                    } else if (winrt::get_abi(existing) == frameAbi) {
                                        found = true;
                                        ++it;
                                    } else {
                                        ++it;
                                    }
                                }
                                if (!found) {
                                    g_scannedFrames.push_back(winrt::make_weak(current));
                                    needsScan = true;
                                }
                            }
                            
                            if (needsScan) ScanAndInjectRecursive(current);
                            break;
                        }
                        
                        auto parent = Media::VisualTreeHelper::GetParent(current);
                        current = parent ? parent.try_as<FrameworkElement>() : nullptr;
                    }
                });
        }
    } catch (...) {
        t_scanPending = false;
    }
}
```

## Example 3: domoticz_domoticz

- Commit: `0fb20ab21cac`
- File: `main/dzVents.cpp`
- Function: `CdzVents::processLuaCommand`
- Score: `49 -> 148` (`+99`)
- Branch delta: `55`, nesting delta: `2`, deep lines delta: `36`
- Manual verdict: `TP`
- Why: existing Lua command dispatcher grew new `SendNotification` and `SendEmail` parsing paths inside the wrapped-command branch. It is authored application code and the added branches materially increase local decision logic.

### Diff

```diff
--- before
+++ after
@@ -21,7 +21,7 @@
 			scriptTrue = TriggerCustomEvent(lua_state, vLuaTable);
 		else
 		{
-			// Check if this is a wrapped string command (device switch command)
+			// Check if this is a wrapped string command (device switch command, notification, email)
 			// Format: { _value = "On", _scriptName = "MyScript" }
 			std::string wrappedValue;
 			std::string scriptName;
@@ -42,14 +42,71 @@
 
 			if (isWrappedCommand)
 			{
-				// This is a device command wrapped with script name
-				// Pass it to EventSystem with "dzVents/" prefix so it appears correctly in logs
-				std::string useScriptName;
-				if (!scriptName.empty())
-					useScriptName = "dzVents/" + scriptName;
+				// Handle wrapped commands: device commands, SendNotification, SendEmail
+				if (lCommand == "SendNotification")
+				{
+					// Parse the notification string format: subject#message#priority#sound#extra#subsystem
+					std::string subject, body, priority("0"), sound, subsystem;
+					std::string extraData;
+					std::vector<std::string> aParam;
+					StringSplit(wrappedValue, "#", aParam);
+					subject = body = aParam[0];
+					if (aParam.size() > 1) {
+						if (!aParam[1].empty())
+							body = aParam[1];
+					}
+					if (aParam.size() > 2) {
+						priority = aParam[2];
... snippet trimmed ...
+				else if (lCommand == "SendEmail")
+				{
+					// Parse the email string format: subject#body#to
+					std::string subject, body, to;
+					std::vector<std::string> aParam;
+					StringSplit(wrappedValue, "#", aParam);
+					if (aParam.size() != 3)
+					{
+						//Invalid
+						_log.Log(LOG_ERROR, "EventSystem: SendEmail, not enough parameters!");
+						return false;
+					}
+					subject = aParam[0];
+					body = aParam[1];
+					stdreplace(body, "\\n", "<br>");
+					to = aParam[2];
+					m_sql.AddTaskItem(_tTaskItem::SendEmailTo(1, subject, body, to));
+					scriptTrue = true;
+				}
 				else
-					useScriptName = filename; // fallback to full path if no script name
-				scriptTrue = m_mainworker.m_eventsystem.ScheduleEvent(lCommand, wrappedValue, useScriptName);
+				{
+					// This is a device command wrapped with script name
+					// Pass it to EventSystem with "dzVents/" prefix so it appears correctly in logs
+					std::string useScriptName;
+					if (!scriptName.empty())
+						useScriptName = "dzVents/" + scriptName;
+					else
+						useScriptName = filename; // fallback to full path if no script name
+					scriptTrue = m_mainworker.m_eventsystem.ScheduleEvent(lCommand, wrappedValue, useScriptName);
+				}
 			}
 		}
 	}
```

### Before

```cpp
{
	bool scriptTrue = false;
	std::string lCommand = std::string(lua_tostring(lua_state, -2));
	std::vector<_tLuaTableValues> vLuaTable;
	IterateTable(lua_state, tIndex, vLuaTable);
	if (!vLuaTable.empty())
	{
		if (lCommand == "OpenURL")
			scriptTrue = OpenURL(lua_state, vLuaTable);
		else if (lCommand == "ExecuteShellCommand")
			scriptTrue = ExecuteShellCommand(lua_state, vLuaTable);
		else if (lCommand == "UpdateDevice")
			scriptTrue = UpdateDevice(lua_state, vLuaTable, filename);
		else if (lCommand == "Variable")
			scriptTrue = UpdateVariable(lua_state, vLuaTable);
		else if (lCommand == "Cancel")
			scriptTrue = CancelItem(lua_state, vLuaTable, filename);
		else if (lCommand == "TriggerIFTTT")
			scriptTrue = TriggerIFTTT(lua_state, vLuaTable);
		else if (lCommand == "CustomEvent")
			scriptTrue = TriggerCustomEvent(lua_state, vLuaTable);
		else
		{
			// Check if this is a wrapped string command (device switch command)
			// Format: { _value = "On", _scriptName = "MyScript" }
			std::string wrappedValue;
			std::string scriptName;
			bool isWrappedCommand = false;

			for (const auto& item : vLuaTable)
			{
				if (item.type == TYPE_STRING && item.name == "_value")
				{
					wrappedValue = item.sValue;
					isWrappedCommand = true;
				}
				else if (item.type == TYPE_STRING && item.name == "_scriptName")
				{
					scriptName = item.sValue;
				}
			}

			if (isWrappedCommand)
			{
				// This is a device command wrapped with script name
				// Pass it to EventSystem with "dzVents/" prefix so it appears correctly in logs
				std::string useScriptName;
				if (!scriptName.empty())
					useScriptName = "dzVents/" + scriptName;
				else
					useScriptName = filename; // fallback to full path if no script name
				scriptTrue = m_mainworker.m_eventsystem.ScheduleEvent(lCommand, wrappedValue, useScriptName);
			}
		}
	}
	return scriptTrue;
}
```

### After

```cpp
{
	bool scriptTrue = false;
	std::string lCommand = std::string(lua_tostring(lua_state, -2));
	std::vector<_tLuaTableValues> vLuaTable;
	IterateTable(lua_state, tIndex, vLuaTable);
	if (!vLuaTable.empty())
	{
		if (lCommand == "OpenURL")
			scriptTrue = OpenURL(lua_state, vLuaTable);
		else if (lCommand == "ExecuteShellCommand")
			scriptTrue = ExecuteShellCommand(lua_state, vLuaTable);
		else if (lCommand == "UpdateDevice")
			scriptTrue = UpdateDevice(lua_state, vLuaTable, filename);
		else if (lCommand == "Variable")
			scriptTrue = UpdateVariable(lua_state, vLuaTable);
		else if (lCommand == "Cancel")
			scriptTrue = CancelItem(lua_state, vLuaTable, filename);
		else if (lCommand == "TriggerIFTTT")
			scriptTrue = TriggerIFTTT(lua_state, vLuaTable);
		else if (lCommand == "CustomEvent")
			scriptTrue = TriggerCustomEvent(lua_state, vLuaTable);
		else
		{
			// Check if this is a wrapped string command (device switch command, notification, email)
			// Format: { _value = "On", _scriptName = "MyScript" }
			std::string wrappedValue;
			std::string scriptName;
			bool isWrappedCommand = false;

			for (const auto& item : vLuaTable)
			{
				if (item.type == TYPE_STRING && item.name == "_value")
				{
					wrappedValue = item.sValue;
					isWrappedCommand = true;
... snippet trimmed ...
				else if (lCommand == "SendEmail")
				{
					// Parse the email string format: subject#body#to
					std::string subject, body, to;
					std::vector<std::string> aParam;
					StringSplit(wrappedValue, "#", aParam);
					if (aParam.size() != 3)
					{
						//Invalid
						_log.Log(LOG_ERROR, "EventSystem: SendEmail, not enough parameters!");
						return false;
					}
					subject = aParam[0];
					body = aParam[1];
					stdreplace(body, "\\n", "<br>");
					to = aParam[2];
					m_sql.AddTaskItem(_tTaskItem::SendEmailTo(1, subject, body, to));
					scriptTrue = true;
				}
				else
				{
					// This is a device command wrapped with script name
					// Pass it to EventSystem with "dzVents/" prefix so it appears correctly in logs
					std::string useScriptName;
					if (!scriptName.empty())
						useScriptName = "dzVents/" + scriptName;
					else
						useScriptName = filename; // fallback to full path if no script name
					scriptTrue = m_mainworker.m_eventsystem.ScheduleEvent(lCommand, wrappedValue, useScriptName);
				}
			}
		}
	}
	return scriptTrue;
}
```

## Example 4: community-shaders_skyrim-community-shaders

- Commit: `3791d86aa91a`
- File: `src/Features/WetnessEffects.cpp`
- Function: `WetnessEffects::DrawSettings`
- Score: `54 -> 142` (`+88`)
- Branch delta: `45`, nesting delta: `2`, deep lines delta: `36`
- Manual verdict: `TP`
- Why: existing ImGui settings renderer grew climate preset selection, bounds checks, preset application, tooltip branches, and feature navigation. Caveat: UI/settings functions are naturally branchy, but this is still real growth inside one function, not a scanner FP.

### Diff

```diff
--- before
+++ after
@@ -1,4 +1,68 @@
 {
+	// Climate Preset Selection - Always visible at the top
+	Util::DrawSectionHeader("Climate Presets", false, false);
+
+	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.3f, 0.4f, 0.6f));    // Subtle blue background
+	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.45f, 0.8f));  // Slightly darker for button
+
+	// Extract names for combo box
+	const char* presetNames[CLIMATE_PRESET_INFO.size()];
+	for (size_t i = 0; i < CLIMATE_PRESET_INFO.size(); ++i) {
+		presetNames[i] = CLIMATE_PRESET_INFO[i].name;
+	}
+	// Map preset enum to combo index (Custom=0, Legacy=1, Nordic=2, Arctic=3, Coastal=4, Monsoon=5)
+	int currentComboIndex = static_cast<int>(climatePreset);
+
+	if (ImGui::Combo("Climate Preset", &currentComboIndex, presetNames, static_cast<int>(CLIMATE_PRESET_INFO.size()))) {  // Map combo index back to preset enum
+		// Simplified: map combo index directly to enum, with bounds check
+		ClimatePreset newPreset = (currentComboIndex >= 0 && currentComboIndex < static_cast<int>(CLIMATE_PRESET_INFO.size())) ? static_cast<ClimatePreset>(currentComboIndex) : defaultPreset;
+
+		// Update the preset selection
+		climatePreset = newPreset;
+
+		// Apply preset settings (but not for Custom, which just means user-modified)
+		if (newPreset != ClimatePreset::Custom) {
+			ApplyClimatePreset(newPreset);
+		}
+	}
+
+	ImGui::PopStyleColor(2);  // Pop both style colors
+	if (auto _tt = Util::HoverTooltipWrapper()) {
+		if (currentComboIndex >= 0 && currentComboIndex < static_cast<int>(CLIMATE_PRESET_INFO.size())) {
+			const auto& info = CLIMATE_PRESET_INFO[currentComboIndex];
... snippet trimmed ...
+				std::format("{:.2f} meters", Util::Units::GameUnitsToMeters(settings.PuddleRadius))
+			};
+			Util::DrawMultiLineTooltip(tooltipLines);
 		}
 
 		ImGui::SliderFloat("Puddle Max Angle", &settings.PuddleMaxAngle, 0.6f, 1.0f);
 		if (auto _tt = Util::HoverTooltipWrapper()) {
-			ImGui::Text(
-				"How flat a surface needs to be for puddles to form on it. ");
+			ImGui::Text("How flat a surface needs to be for puddles to form on it.");
 		}
 
 		ImGui::SliderFloat("Puddle Min Wetness", &settings.PuddleMinWetness, 0.0f, 1.0f);
 		if (auto _tt = Util::HoverTooltipWrapper()) {
-			ImGui::Text(
-				"The wetness value at which puddles start to form. ");
+			ImGui::Text("The wetness value at which puddles start to form.");
 		}
 
 		ImGui::TreePop();
@@ -135,4 +225,14 @@
 
 	ImGui::Spacing();
 	ImGui::Spacing();
+	auto weather = globals::features::weatherPicker;
+	if (weather && weather->loaded) {
+		if (ImGui::SmallButton(("Open " + weather->GetName()).c_str())) {
+			// Navigate to the replacement feature in the menu
+			Menu::GetSingleton()->SelectFeatureMenu(weather->GetShortName());
+		}
+		if (auto _tt = Util::HoverTooltipWrapper()) {
+			ImGui::Text("Open the installed %s feature", weather->GetShortName().c_str());
+		}
+	}
 }
```

### Before

```cpp
{
	if (ImGui::TreeNodeEx("Wetness Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Checkbox("Enable Wetness", (bool*)&settings.EnableWetnessEffects)) {
			Ripples::UpdateSettings();  // Update cache when settings change
		}
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Enables a wetness effect near water and when it is raining.");
		}

		ImGui::SliderFloat("Rain Wetness", &settings.MaxRainWetness, 0.0f, 1.0f);
		ImGui::SliderFloat("Puddle Wetness", &settings.MaxPuddleWetness, 0.0f, 4.0f);
		ImGui::SliderFloat("Shore Wetness", &settings.MaxShoreWetness, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	if (ImGui::TreeNodeEx("Raindrop Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Raindrop Effects", (bool*)&settings.EnableRaindropFx);

		ImGui::BeginDisabled(!settings.EnableRaindropFx);

		ImGui::Checkbox("Enable Splashes", (bool*)&settings.EnableSplashes);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("Enables small splashes of wetness on dry surfaces.");
		ImGui::Checkbox("Enable Ripples", (bool*)&settings.EnableRipples);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("Enables circular ripples on puddles, and to a less extent other wet surfaces");

		ImGui::BeginDisabled(splashesOfStormsLoaded);
		std::string checkboxLabel = splashesOfStormsLoaded ?
		                                "Enable Vanilla Ripples - Controlled by Splashes of Storms" :
		                                "Enable Vanilla Ripples";

... snippet trimmed ...
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"How wet character skin and hair get during rain. ");
		}

		ImGui::SliderInt("Shore Range", (int*)&settings.ShoreRange, 1, 64);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"The maximum distance from a body of water that Shore Wetness affects. ");
		}

		ImGui::SliderFloat("Puddle Radius", &settings.PuddleRadius, 0.3f, 3.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"The radius that is used to determine puddle size and location. ");
		}

		ImGui::SliderFloat("Puddle Max Angle", &settings.PuddleMaxAngle, 0.6f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"How flat a surface needs to be for puddles to form on it. ");
		}

		ImGui::SliderFloat("Puddle Min Wetness", &settings.PuddleMinWetness, 0.0f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text(
				"The wetness value at which puddles start to form. ");
		}

		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Spacing();
}
```

### After

```cpp
{
	// Climate Preset Selection - Always visible at the top
	Util::DrawSectionHeader("Climate Presets", false, false);

	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.3f, 0.4f, 0.6f));    // Subtle blue background
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.45f, 0.8f));  // Slightly darker for button

	// Extract names for combo box
	const char* presetNames[CLIMATE_PRESET_INFO.size()];
	for (size_t i = 0; i < CLIMATE_PRESET_INFO.size(); ++i) {
		presetNames[i] = CLIMATE_PRESET_INFO[i].name;
	}
	// Map preset enum to combo index (Custom=0, Legacy=1, Nordic=2, Arctic=3, Coastal=4, Monsoon=5)
	int currentComboIndex = static_cast<int>(climatePreset);

	if (ImGui::Combo("Climate Preset", &currentComboIndex, presetNames, static_cast<int>(CLIMATE_PRESET_INFO.size()))) {  // Map combo index back to preset enum
		// Simplified: map combo index directly to enum, with bounds check
		ClimatePreset newPreset = (currentComboIndex >= 0 && currentComboIndex < static_cast<int>(CLIMATE_PRESET_INFO.size())) ? static_cast<ClimatePreset>(currentComboIndex) : defaultPreset;

		// Update the preset selection
		climatePreset = newPreset;

		// Apply preset settings (but not for Custom, which just means user-modified)
		if (newPreset != ClimatePreset::Custom) {
			ApplyClimatePreset(newPreset);
		}
	}

	ImGui::PopStyleColor(2);  // Pop both style colors
	if (auto _tt = Util::HoverTooltipWrapper()) {
		if (currentComboIndex >= 0 && currentComboIndex < static_cast<int>(CLIMATE_PRESET_INFO.size())) {
			const auto& info = CLIMATE_PRESET_INFO[currentComboIndex];

			// Handle Custom preset differently
			if (currentComboIndex == 0) {  // Custom preset
... snippet trimmed ...
		if (auto _tt = Util::HoverTooltipWrapper()) {
			std::vector<std::string> tooltipLines = {
				"The radius used to determine puddle size and location",
				Util::Units::FormatDistance(settings.PuddleRadius),
				std::format("{:.2f} meters", Util::Units::GameUnitsToMeters(settings.PuddleRadius))
			};
			Util::DrawMultiLineTooltip(tooltipLines);
		}

		ImGui::SliderFloat("Puddle Max Angle", &settings.PuddleMaxAngle, 0.6f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("How flat a surface needs to be for puddles to form on it.");
		}

		ImGui::SliderFloat("Puddle Min Wetness", &settings.PuddleMinWetness, 0.0f, 1.0f);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("The wetness value at which puddles start to form.");
		}

		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Spacing();
	auto weather = globals::features::weatherPicker;
	if (weather && weather->loaded) {
		if (ImGui::SmallButton(("Open " + weather->GetName()).c_str())) {
			// Navigate to the replacement feature in the menu
			Menu::GetSingleton()->SelectFeatureMenu(weather->GetShortName());
		}
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Open the installed %s feature", weather->GetShortName().c_str());
		}
	}
}
```

## Example 5: community-shaders_skyrim-community-shaders

- Commit: `b225568e3228`
- File: `src/Upscaling.cpp`
- Function: `Upscaling::DrawSettings`
- Score: `55 -> 137` (`+82`)
- Branch delta: `44`, nesting delta: `1`, deep lines delta: `33`
- Manual verdict: `TP`
- Why: existing settings renderer grew frame-generation availability gating plus backend diagnostics with loops, selectable actions, sorting setup, and multiple UI conditionals. Caveat: like Example 4, it is UI-heavy code, but the signal correctly identifies in-function complexity growth.

### Diff

```diff
--- before
+++ after
@@ -69,53 +69,107 @@
 	}
 
 	if (!globals::game::isVR) {
-		if (ImGui::TreeNodeEx("Frame Generation", ImGuiTreeNodeFlags_DefaultOpen)) {
-			ImGui::Text("Frame Generation interpolates real frames with generated ones for a smoother experience");
-			ImGui::Text("Uses AMD FSR 3.1 Frame Generation and NVIDIA DLSS Frame Generation technology");
-			ImGui::Text("Requires a D3D11 to D3D12 proxy which can create compatibility issues");
-			ImGui::Text("Toggling this setting requires a restart to work correctly");
+		bool frameGenAvailable = d3d12Interop && ((globals::streamline && globals::streamline->featureDLSSG) ||
+													 (globals::fidelityFX && globals::fidelityFX->featureFSR3FG));
+		if (frameGenAvailable) {
+			if (ImGui::TreeNodeEx("Frame Generation", ImGuiTreeNodeFlags_DefaultOpen)) {
+				ImGui::Text("Frame Generation interpolates real frames with generated ones for a smoother experience");
+				ImGui::Text("Uses AMD FSR 3.1 Frame Generation and NVIDIA DLSS Frame Generation technology");
+				if (globals::streamline && globals::streamline->featureDLSSG)
+					ImGui::Text("NVIDIA DLSS-G Frame Generation is available.");
+				if (globals::fidelityFX && globals::fidelityFX->featureFSR3FG)
+					ImGui::Text("AMD FSR 3.1 Frame Generation is available.");
+				ImGui::Text("Requires a D3D11 to D3D12 proxy which can create compatibility issues");
+				ImGui::Text("Toggling this setting requires a restart to work correctly");
 
-			bool onlyRequiresRestart = true;
+				bool onlyRequiresRestart = true;
 
-			if (!isWindowed) {
-				ImGui::Text("Warning: Requires windowed mode");
-				onlyRequiresRestart = false;
+				if (!isWindowed) {
+					ImGui::Text("Warning: Requires windowed mode");
+					onlyRequiresRestart = false;
+				}
+
... snippet trimmed ...
+	if (ImGui::TreeNodeEx("Backend Diagnostics")) {
+		// Streamline log level selection
+		const char* logLevels[] = { "Off", "Default", "Verbose" };
+		int logLevelIdx = static_cast<int>(settings.streamlineLogLevel);
+		if (ImGui::Combo("Streamline Logging", &logLevelIdx, logLevels, IM_ARRAYSIZE(logLevels))) {
+			settings.streamlineLogLevel = static_cast<uint>(logLevelIdx);
+		}
+		ImGui::TextUnformatted("Changing this requires a restart to take effect.");
+		if (auto _tt = Util::HoverTooltipWrapper()) {
+			ImGui::Text("Streamline logging controls the verbosity of NVIDIA Streamline backend logs. Useful for debugging issues with DLSS/DLSS-G.");
+		}
+		ImGui::Separator();
+		// FidelityFX section
+		if (ImGui::Selectable("AMD FidelityFX DLLs (click to open folder)")) {
+			ShellExecuteW(nullptr, L"open", FidelityFX::PluginDir, nullptr, nullptr, SW_SHOWNORMAL);
+		}
+		std::vector<std::string> headers = { "DLL Name", "Version" };
+		std::vector<std::vector<std::string>> ffRows;
+		for (const auto& [name, version] : FidelityFX::dllVersions)
+			ffRows.push_back({ name, version });
+		std::vector<Util::TableSortFunc> ffSorters = { nullptr, Util::VersionSortComparator };
+		Util::ShowSortedStringTable("ffx_dll_versions", headers, ffRows, 0, true, ffSorters);
+
+		// Streamline section
+		if (ImGui::Selectable("NVIDIA Streamline DLLs (click to open folder)")) {
+			ShellExecuteW(nullptr, L"open", Streamline::PluginDir, nullptr, nullptr, SW_SHOWNORMAL);
+		}
+		std::vector<std::vector<std::string>> slRows;
+		for (const auto& [name, version] : Streamline::dllVersions)
+			slRows.push_back({ name, version });
+		std::vector<Util::TableSortFunc> slSorters = { nullptr, Util::VersionSortComparator };
+		Util::ShowSortedStringTable("sl_dll_versions", headers, slRows, 0, true, slSorters);
+		ImGui::TreePop();
+	}
 }
```

### Before

```cpp
{
	// Skyrim settings control whether any upscaling is possible

	auto state = globals::state;
	auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();
	auto streamline = globals::streamline;
	GET_INSTANCE_MEMBER(BSImagespaceShaderISTemporalAA, imageSpaceManager);
	auto& bTAA = BSImagespaceShaderISTemporalAA->taaEnabled;  // Setting used by shaders

	// Update upscale mode based on TAA setting
	settings.upscaleMethod = bTAA ? (settings.upscaleMethod == (uint)UpscaleMethod::kNONE ? (uint)UpscaleMethod::kTAA : settings.upscaleMethod) : (uint)UpscaleMethod::kNONE;

	// Display upscaling options in the UI
	const char* upscaleModes[] = { "Disabled", "Temporal Anti-Aliasing", "AMD FSR 3.1", "NVIDIA DLAA" };

	// Determine available modes
	bool featureDLSS = streamline->featureDLSS;
	uint* currentUpscaleMode = featureDLSS ? &settings.upscaleMethod : &settings.upscaleMethodNoDLSS;
	uint availableModes = (globals::game::isVR && state->upscalerLoaded) ? (featureDLSS ? 2 : 1) : (featureDLSS ? 3 : 2);

	if (state->featureLevel != D3D_FEATURE_LEVEL_11_1)
		availableModes = 1;

	// Slider for method selection
	ImGui::SliderInt("Method", (int*)currentUpscaleMode, 0, availableModes, std::format("{}", upscaleModes[(uint)*currentUpscaleMode]).c_str());
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text(
			"Disabled:\n"
			"Disable all methods. Same as disabling Skyrim's TAA.\n"
			"\n"
			"Temporal Anti-Aliasing:\n"
			"Uses Skyrim's TAA which uses frame history to smooth out jagged edges, reducing flickering and improving image stability.\n"
			"\n"
			"AMD FSR 3.1:\n"
			"AMD's open-source FSR spatial upscaling algorithm designed to enhance performance while maintaining high visual quality.\n"
... snippet trimmed ...
				onlyRequiresRestart = false;
			}

			if (streamlineMissing) {
				ImGui::Text("Warning: amd_fidelityfx_dx12.dll is not loaded");
				onlyRequiresRestart = false;
			}

			if (fidelityFXMissing) {
				ImGui::Text("Warning: Streamline is not loaded");
				onlyRequiresRestart = false;
			}

			if (onlyRequiresRestart && settings.frameGenerationMode && !d3d12Interop)
				ImGui::Text("Warning: Requires restart");

			const char* toggleModes[] = { "Disabled", "Enabled" };

			ImGui::SliderInt("Frame Generation", (int*)&settings.frameGenerationMode, 0, 1, std::format("{}", toggleModes[settings.frameGenerationMode]).c_str());

			if (!d3d12Interop)
				ImGui::BeginDisabled();

			ImGui::SliderInt("Frame Limit (Variable Refresh Rate)", (int*)&settings.frameLimitMode, 0, 1, std::format("{}", toggleModes[settings.frameLimitMode]).c_str());

			if (!d3d12Interop)
				ImGui::EndDisabled();

			ImGui::Text("Allows frame generation to function on low refresh rate monitors");
			ImGui::SliderInt("Force Enable Frame Generation", (int*)&settings.frameGenerationForceEnable, 0, 1, std::format("{}", toggleModes[settings.frameGenerationForceEnable]).c_str());

			ImGui::TreePop();
		}
	}
}
```

### After

```cpp
{
	// Skyrim settings control whether any upscaling is possible

	auto state = globals::state;
	auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();
	auto streamline = globals::streamline;
	GET_INSTANCE_MEMBER(BSImagespaceShaderISTemporalAA, imageSpaceManager);
	auto& bTAA = BSImagespaceShaderISTemporalAA->taaEnabled;  // Setting used by shaders

	// Update upscale mode based on TAA setting
	settings.upscaleMethod = bTAA ? (settings.upscaleMethod == (uint)UpscaleMethod::kNONE ? (uint)UpscaleMethod::kTAA : settings.upscaleMethod) : (uint)UpscaleMethod::kNONE;

	// Display upscaling options in the UI
	const char* upscaleModes[] = { "Disabled", "Temporal Anti-Aliasing", "AMD FSR 3.1", "NVIDIA DLAA" };

	// Determine available modes
	bool featureDLSS = streamline->featureDLSS;
	uint* currentUpscaleMode = featureDLSS ? &settings.upscaleMethod : &settings.upscaleMethodNoDLSS;
	uint availableModes = (globals::game::isVR && state->upscalerLoaded) ? (featureDLSS ? 2 : 1) : (featureDLSS ? 3 : 2);

	if (state->featureLevel != D3D_FEATURE_LEVEL_11_1)
		availableModes = 1;

	// Slider for method selection
	ImGui::SliderInt("Method", (int*)currentUpscaleMode, 0, availableModes, std::format("{}", upscaleModes[(uint)*currentUpscaleMode]).c_str());
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text(
			"Disabled:\n"
			"Disable all methods. Same as disabling Skyrim's TAA.\n"
			"\n"
			"Temporal Anti-Aliasing:\n"
			"Uses Skyrim's TAA which uses frame history to smooth out jagged edges, reducing flickering and improving image stability.\n"
			"\n"
			"AMD FSR 3.1:\n"
			"AMD's open-source FSR spatial upscaling algorithm designed to enhance performance while maintaining high visual quality.\n"
... snippet trimmed ...
	if (ImGui::TreeNodeEx("Backend Diagnostics")) {
		// Streamline log level selection
		const char* logLevels[] = { "Off", "Default", "Verbose" };
		int logLevelIdx = static_cast<int>(settings.streamlineLogLevel);
		if (ImGui::Combo("Streamline Logging", &logLevelIdx, logLevels, IM_ARRAYSIZE(logLevels))) {
			settings.streamlineLogLevel = static_cast<uint>(logLevelIdx);
		}
		ImGui::TextUnformatted("Changing this requires a restart to take effect.");
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Streamline logging controls the verbosity of NVIDIA Streamline backend logs. Useful for debugging issues with DLSS/DLSS-G.");
		}
		ImGui::Separator();
		// FidelityFX section
		if (ImGui::Selectable("AMD FidelityFX DLLs (click to open folder)")) {
			ShellExecuteW(nullptr, L"open", FidelityFX::PluginDir, nullptr, nullptr, SW_SHOWNORMAL);
		}
		std::vector<std::string> headers = { "DLL Name", "Version" };
		std::vector<std::vector<std::string>> ffRows;
		for (const auto& [name, version] : FidelityFX::dllVersions)
			ffRows.push_back({ name, version });
		std::vector<Util::TableSortFunc> ffSorters = { nullptr, Util::VersionSortComparator };
		Util::ShowSortedStringTable("ffx_dll_versions", headers, ffRows, 0, true, ffSorters);

		// Streamline section
		if (ImGui::Selectable("NVIDIA Streamline DLLs (click to open folder)")) {
			ShellExecuteW(nullptr, L"open", Streamline::PluginDir, nullptr, nullptr, SW_SHOWNORMAL);
		}
		std::vector<std::vector<std::string>> slRows;
		for (const auto& [name, version] : Streamline::dllVersions)
			slRows.push_back({ name, version });
		std::vector<Util::TableSortFunc> slSorters = { nullptr, Util::VersionSortComparator };
		Util::ShowSortedStringTable("sl_dll_versions", headers, slRows, 0, true, slSorters);
		ImGui::TreePop();
	}
}
```

## Example 6: ramensoftware_windhawk-mods

- Commit: `7b8cbb03327c`
- File: `mods/transparent-desktop-icons-spotlight.wh.cpp`
- Function: `DispatchMessageW_Hook`
- Score: `6 -> 85` (`+79`)
- Branch delta: `40`, nesting delta: `6`, deep lines delta: `32`
- Manual verdict: `TP`
- Why: existing Windows message hook changed from two simple initialization paths into an init/backoff/discovery state machine with nested failure handling, event wake-up, throttling, and `PostThreadMessage` discovery. This is a strong true positive.

### Diff

```diff
--- before
+++ after
@@ -1,16 +1,62 @@
 LRESULT WINAPI DispatchMessageW_Hook(const MSG* lpMsg) {
-    if (!g_initialized_desktop && g_desktopThreadId != 0 && GetCurrentThreadId() == g_desktopThreadId) {
-        g_initialized_desktop = true;
-        CreateOverlayWindow();
-        CreateMessageWindow();
+    if (!g_unloading) {
+        // 1. Handling Initialization Execution (Runs safely on target desktop thread)
+        if (lpMsg->message == WM_APP_INIT && lpMsg->hwnd == nullptr) {
+            CreateMessageWindow();
+            if (g_messageWnd) {
+                CreateOverlayWindow();
+                if (g_overlayWnd) {
+                    g_initAttempts.store(0, std::memory_order_relaxed);
+                    g_lastInitFailure.store(0, std::memory_order_relaxed);
+                } else {
+                    g_initAttempts.fetch_add(1, std::memory_order_relaxed);
+                    g_lastInitFailure.store(GetTickCount64(), std::memory_order_relaxed);
+                    g_initialized_desktop = false; // Reset to allow discovery again
+                }
+            } else {
+                g_initAttempts.fetch_add(1, std::memory_order_relaxed);
+                g_lastInitFailure.store(GetTickCount64(), std::memory_order_relaxed);
+                g_initialized_desktop = false; // Reset to allow discovery again
+            }
+        }
+        // 2. Hardware-level Mouse Instant Wake
+        else if (g_renderEvent &&
+                 ((lpMsg->message >= WM_MOUSEFIRST && lpMsg->message <= WM_MOUSELAST) ||
+                  lpMsg->message == 0x0245 /* WM_POINTERUPDATE */)) {
+            SetEvent(g_renderEvent);
+        }
+
... snippet trimmed ...
+            bool backoffActive = (useLongBackoff && lastFail != 0 && (now - lastFail) < 5000);
+
+            if (!backoffActive) {
+                static std::atomic<ULONGLONG> s_lastCheck{0};
+                ULONGLONG last = s_lastCheck.load(std::memory_order_relaxed);
+
+                // Throttle searches to every 500ms
+                if (now - last >= 500) {
+                    s_lastCheck.store(now, std::memory_order_relaxed);
+
+                    HWND hLV = GetDesktopListView();
+                    if (hLV && IsWindowVisible(hLV)) {
+                        DWORD tid = GetWindowThreadProcessId(hLV, nullptr);
+                        if (tid != 0) {
+                            // We found the desktop! ANY thread can safely post this 
+                            // message directly to the desktop's thread queue.
+                            if (PostThreadMessage(tid, WM_APP_INIT, 0, 0)) {
+                                g_initialized_desktop = true; 
+                            }
+                        }
+                    }
+                }
+            }
+        }
     }
-    
-    // Catch the safe bootstrap timer
-    if (lpMsg->message == WM_TIMER && lpMsg->wParam == 0x1337 && IsFolderViewWnd(lpMsg->hwnd)) {
-        KillTimer(lpMsg->hwnd, 0x1337);
-        CreateOverlayWindow();
-        CreateMessageWindow();
-    }
-
     return DispatchMessageW_Original(lpMsg);
 }
```

### Before

```cpp
LRESULT WINAPI DispatchMessageW_Hook(const MSG* lpMsg) {
    if (!g_initialized_desktop && g_desktopThreadId != 0 && GetCurrentThreadId() == g_desktopThreadId) {
        g_initialized_desktop = true;
        CreateOverlayWindow();
        CreateMessageWindow();
    }
    
    // Catch the safe bootstrap timer
    if (lpMsg->message == WM_TIMER && lpMsg->wParam == 0x1337 && IsFolderViewWnd(lpMsg->hwnd)) {
        KillTimer(lpMsg->hwnd, 0x1337);
        CreateOverlayWindow();
        CreateMessageWindow();
    }

    return DispatchMessageW_Original(lpMsg);
}
```

### After

```cpp
LRESULT WINAPI DispatchMessageW_Hook(const MSG* lpMsg) {
    if (!g_unloading) {
        // 1. Handling Initialization Execution (Runs safely on target desktop thread)
        if (lpMsg->message == WM_APP_INIT && lpMsg->hwnd == nullptr) {
            CreateMessageWindow();
            if (g_messageWnd) {
                CreateOverlayWindow();
                if (g_overlayWnd) {
                    g_initAttempts.store(0, std::memory_order_relaxed);
                    g_lastInitFailure.store(0, std::memory_order_relaxed);
                } else {
                    g_initAttempts.fetch_add(1, std::memory_order_relaxed);
                    g_lastInitFailure.store(GetTickCount64(), std::memory_order_relaxed);
                    g_initialized_desktop = false; // Reset to allow discovery again
                }
            } else {
                g_initAttempts.fetch_add(1, std::memory_order_relaxed);
                g_lastInitFailure.store(GetTickCount64(), std::memory_order_relaxed);
                g_initialized_desktop = false; // Reset to allow discovery again
            }
        }
        // 2. Hardware-level Mouse Instant Wake
        else if (g_renderEvent &&
                 ((lpMsg->message >= WM_MOUSEFIRST && lpMsg->message <= WM_MOUSELAST) ||
                  lpMsg->message == 0x0245 /* WM_POINTERUPDATE */)) {
            SetEvent(g_renderEvent);
        }

        // 3. Desktop Window Discovery (Can be safely executed by ANY thread)
        if (!g_initialized_desktop) {
            ULONGLONG now = GetTickCount64();

            // Backoff logic: If we failed 5+ times, wait 5 seconds between searches
            bool useLongBackoff = g_initAttempts.load(std::memory_order_relaxed) > 5;
            ULONGLONG lastFail = g_lastInitFailure.load(std::memory_order_relaxed);
            bool backoffActive = (useLongBackoff && lastFail != 0 && (now - lastFail) < 5000);

            if (!backoffActive) {
                static std::atomic<ULONGLONG> s_lastCheck{0};
                ULONGLONG last = s_lastCheck.load(std::memory_order_relaxed);

                // Throttle searches to every 500ms
                if (now - last >= 500) {
                    s_lastCheck.store(now, std::memory_order_relaxed);

                    HWND hLV = GetDesktopListView();
                    if (hLV && IsWindowVisible(hLV)) {
                        DWORD tid = GetWindowThreadProcessId(hLV, nullptr);
                        if (tid != 0) {
                            // We found the desktop! ANY thread can safely post this 
                            // message directly to the desktop's thread queue.
                            if (PostThreadMessage(tid, WM_APP_INIT, 0, 0)) {
                                g_initialized_desktop = true; 
                            }
                        }
                    }
                }
            }
        }
    }
    return DispatchMessageW_Original(lpMsg);
}
```
