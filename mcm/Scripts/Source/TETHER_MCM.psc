Scriptname TETHER_MCM extends SKI_ConfigBase
{TETHER — Hand-Holding for Followers: MCM configuration script.
 Attach to Quest TETHER_MCMQuest (Start Game Enabled).}

; =============================================================================
;  Global variable properties (fill in CK by pointing at the ESP forms).
;  All Floats. Booleans use 0.0 / 1.0. DIK scancode stored as Float.
; =============================================================================
GlobalVariable Property TETHER_Hotkey                Auto  ; DIK scancode (default 35 = H)

GlobalVariable Property TETHER_OffsetRight           Auto  ; follower Right in target-local frame
GlobalVariable Property TETHER_OffsetBack            Auto  ; follower Back
GlobalVariable Property TETHER_FollowRadius          Auto
GlobalVariable Property TETHER_CatchupRadius         Auto

GlobalVariable Property TETHER_TetherIdealDist       Auto
GlobalVariable Property TETHER_TetherSlackDist       Auto
GlobalVariable Property TETHER_PlayerSpeedMultNear   Auto
GlobalVariable Property TETHER_PlayerSpeedMultFar    Auto
GlobalVariable Property TETHER_FollowerSpeedMult     Auto

GlobalVariable Property TETHER_PalmOffset            Auto

GlobalVariable Property TETHER_AutoRelWeapon         Auto  ; bool
GlobalVariable Property TETHER_AutoRelCombat         Auto  ; bool
GlobalVariable Property TETHER_AutoRelExtDist        Auto  ; bool
GlobalVariable Property TETHER_ExtDistThreshold      Auto
GlobalVariable Property TETHER_ExtDistDuration       Auto

; =============================================================================
;  Option IDs (assigned in OnPageReset).
; =============================================================================
Int HotkeyID
Int AutoRelWeaponID
Int AutoRelCombatID
Int AutoRelExtDistID
Int ExtDistThresholdID
Int ExtDistDurationID
Int ResetAllID

Int OffsetRightID
Int OffsetBackID
Int FollowRadiusID
Int CatchupRadiusID

Int TetherIdealID
Int TetherSlackID
Int PSpeedNearID
Int PSpeedFarID
Int FSpeedMultID
Int PalmOffsetID

; =============================================================================
;  Default values.
; =============================================================================
Int Function DEF_Hotkey()
    Return 35
EndFunction

Float Function DEF_OffsetRight()
    Return 0.0
EndFunction

Float Function DEF_OffsetBack()
    Return -30.0
EndFunction

Float Function DEF_FollowRadius()
    Return 30.0
EndFunction

Float Function DEF_CatchupRadius()
    Return 60.0
EndFunction

Float Function DEF_TetherIdeal()
    Return 40.0
EndFunction

Float Function DEF_TetherSlack()
    Return 120.0
EndFunction

Float Function DEF_PSpeedNear()
    Return 1.0
EndFunction

Float Function DEF_PSpeedFar()
    Return 0.35
EndFunction

Float Function DEF_FSpeedMult()
    Return 1.30
EndFunction

Float Function DEF_PalmOffset()
    Return 5.0
EndFunction

Bool Function DEF_AutoRelWeapon()
    Return True
EndFunction

Bool Function DEF_AutoRelCombat()
    Return False
EndFunction

Bool Function DEF_AutoRelExtDist()
    Return False
EndFunction

Float Function DEF_ExtDistThreshold()
    Return 500.0
EndFunction

Float Function DEF_ExtDistDuration()
    Return 3.0
EndFunction

; =============================================================================
;  Lifecycle.
; =============================================================================
Event OnConfigInit()
    ModName = "TETHER"                 ; Name shown in MCM list (SKI_ConfigBase property).
    Pages = New String[3]
    Pages[0] = "General"
    Pages[1] = "Position"
    Pages[2] = "Tension & Grip"
    ApplyAllDefaults()
EndEvent

Event OnVersionUpdate(Int a_version)
    ; Reserved for future settings migrations.
EndEvent

Int Function GetVersion()
    Return 1
EndFunction

; =============================================================================
;  Helpers for boolean globals stored as 0.0 / 1.0.
; =============================================================================
Bool Function GetBool(GlobalVariable g)
    If g == None
        Return False
    EndIf
    Return g.GetValue() != 0.0
EndFunction

Function SetBool(GlobalVariable g, Bool v)
    If g == None
        Return
    EndIf
    If v
        g.SetValue(1.0)
    Else
        g.SetValue(0.0)
    EndIf
EndFunction

Function SetFloat(GlobalVariable g, Float v)
    If g == None
        Return
    EndIf
    g.SetValue(v)
EndFunction

Function SetInt(GlobalVariable g, Int v)
    If g == None
        Return
    EndIf
    g.SetValue(v as Float)
EndFunction

Int Function GetInt(GlobalVariable g)
    If g == None
        Return 0
    EndIf
    Return g.GetValue() as Int
EndFunction

; =============================================================================
;  Apply all defaults (used by OnConfigInit and Reset All).
; =============================================================================
Function ApplyAllDefaults()
    SetInt(TETHER_Hotkey,              DEF_Hotkey())

    SetFloat(TETHER_OffsetRight,       DEF_OffsetRight())
    SetFloat(TETHER_OffsetBack,        DEF_OffsetBack())
    SetFloat(TETHER_FollowRadius,      DEF_FollowRadius())
    SetFloat(TETHER_CatchupRadius,     DEF_CatchupRadius())

    SetFloat(TETHER_TetherIdealDist,   DEF_TetherIdeal())
    SetFloat(TETHER_TetherSlackDist,   DEF_TetherSlack())
    SetFloat(TETHER_PlayerSpeedMultNear, DEF_PSpeedNear())
    SetFloat(TETHER_PlayerSpeedMultFar,  DEF_PSpeedFar())
    SetFloat(TETHER_FollowerSpeedMult,   DEF_FSpeedMult())

    SetFloat(TETHER_PalmOffset,        DEF_PalmOffset())

    SetBool(TETHER_AutoRelWeapon,      DEF_AutoRelWeapon())
    SetBool(TETHER_AutoRelCombat,      DEF_AutoRelCombat())
    SetBool(TETHER_AutoRelExtDist,     DEF_AutoRelExtDist())
    SetFloat(TETHER_ExtDistThreshold,  DEF_ExtDistThreshold())
    SetFloat(TETHER_ExtDistDuration,   DEF_ExtDistDuration())
EndFunction

; =============================================================================
;  Page layout.
; =============================================================================
Event OnPageReset(String page)
    If page == "" || page == "General"
        SetCursorFillMode(TOP_TO_BOTTOM)

        AddHeaderOption("Activation")
        HotkeyID = AddKeyMapOption("Toggle Key", GetInt(TETHER_Hotkey))

        AddHeaderOption("Auto-Release Triggers")
        AutoRelWeaponID  = AddToggleOption("On Weapon Drawn",    GetBool(TETHER_AutoRelWeapon))
        AutoRelCombatID  = AddToggleOption("On Combat Start",    GetBool(TETHER_AutoRelCombat))
        AutoRelExtDistID = AddToggleOption("On Extreme Distance", GetBool(TETHER_AutoRelExtDist))

        Int extFlags = OPTION_FLAG_NONE
        If !GetBool(TETHER_AutoRelExtDist)
            extFlags = OPTION_FLAG_DISABLED
        EndIf
        ExtDistThresholdID = AddSliderOption("   Distance Threshold", TETHER_ExtDistThreshold.GetValue(), "{0}u", extFlags)
        ExtDistDurationID  = AddSliderOption("   Duration",           TETHER_ExtDistDuration.GetValue(),  "{1}s", extFlags)

        SetCursorPosition(1)
        AddHeaderOption("Reset")
        ResetAllID = AddTextOption("Reset All Settings", "")

    ElseIf page == "Position"
        SetCursorFillMode(TOP_TO_BOTTOM)

        AddHeaderOption("Follower Offset (from Player)")
        OffsetRightID   = AddSliderOption("Right (- = Left)", TETHER_OffsetRight.GetValue(),   "{0}u")
        OffsetBackID    = AddSliderOption("Back (- = Behind)", TETHER_OffsetBack.GetValue(),   "{0}u")

        AddHeaderOption("Follow AI Radii")
        FollowRadiusID  = AddSliderOption("Follow Radius",     TETHER_FollowRadius.GetValue(),  "{0}u")
        CatchupRadiusID = AddSliderOption("Catchup Radius",    TETHER_CatchupRadius.GetValue(), "{0}u")

    ElseIf page == "Tension & Grip"
        SetCursorFillMode(TOP_TO_BOTTOM)

        AddHeaderOption("Elastic Tether (ICO-style speed coupling)")
        TetherIdealID = AddSliderOption("Ideal Distance (no cap)",  TETHER_TetherIdealDist.GetValue(),     "{0}u")
        TetherSlackID = AddSliderOption("Slack Distance (max cap)", TETHER_TetherSlackDist.GetValue(),     "{0}u")
        PSpeedNearID  = AddSliderOption("Player Speed Near",        TETHER_PlayerSpeedMultNear.GetValue(), "{2}x")
        PSpeedFarID   = AddSliderOption("Player Speed Far",         TETHER_PlayerSpeedMultFar.GetValue(),  "{2}x")
        FSpeedMultID  = AddSliderOption("Follower Speed Boost",     TETHER_FollowerSpeedMult.GetValue(),   "{2}x")

        AddHeaderOption("Grip Point")
        PalmOffsetID  = AddSliderOption("Palm Offset (into hand)",  TETHER_PalmOffset.GetValue(),          "{1}u")
    EndIf
EndEvent

; =============================================================================
;  Highlight (info text at bottom).
; =============================================================================
Event OnOptionHighlight(Int option)
    If option == HotkeyID
        SetInfoText("Key to toggle the tether. Supports keyboard and gamepad. Default: H")
    ElseIf option == AutoRelWeaponID
        SetInfoText("Release the tether automatically when the player draws a weapon. Default: ON")
    ElseIf option == AutoRelCombatID
        SetInfoText("Release the tether when either actor enters combat. Default: OFF (keep holding hands while fleeing)")
    ElseIf option == AutoRelExtDistID
        SetInfoText("Release when the follower stays far away for a sustained period. Default: OFF (manual release)")
    ElseIf option == ExtDistThresholdID
        SetInfoText("Distance threshold (Skyrim units, ~1.4cm each) for the extreme-distance auto-release. Default: 500u")
    ElseIf option == ExtDistDurationID
        SetInfoText("How many seconds the distance must be exceeded before releasing. Default: 3s")
    ElseIf option == OffsetRightID
        SetInfoText("Right/left offset of follower relative to player. Negative = left. Default: 0")
    ElseIf option == OffsetBackID
        SetInfoText("Forward/back offset. Negative = behind. Default: -30")
    ElseIf option == FollowRadiusID
        SetInfoText("Within this distance from the ideal spot the follower stops moving. Default: 30")
    ElseIf option == CatchupRadiusID
        SetInfoText("Beyond this distance the follower switches to catch-up run. Default: 60")
    ElseIf option == TetherIdealID
        SetInfoText("Within this player-follower distance, no speed cap. Default: 40")
    ElseIf option == TetherSlackID
        SetInfoText("Beyond this distance, player speed is capped to walk speed. Between ideal and slack it lerps. Default: 120")
    ElseIf option == PSpeedNearID
        SetInfoText("Player speed multiplier when tether is close (0.5-1.0). Default: 1.00")
    ElseIf option == PSpeedFarID
        SetInfoText("Player speed multiplier when tether is stretched to slack limit. Default: 0.35 (walk speed)")
    ElseIf option == FSpeedMultID
        SetInfoText("Follower speed multiplier while tethered. Default: 1.30 (30% faster to keep up)")
    ElseIf option == PalmOffsetID
        SetInfoText("How deep into the palm the constraint attaches. 0 = wrist. Default: 5")
    ElseIf option == ResetAllID
        SetInfoText("Restore all TETHER settings to their default values.")
    EndIf
EndEvent

; =============================================================================
;  Slider open (defines range/step/default for the slider dialog).
; =============================================================================
Event OnOptionSliderOpen(Int option)
    If option == ExtDistThresholdID
        SetSliderDialogStartValue(TETHER_ExtDistThreshold.GetValue())
        SetSliderDialogDefaultValue(DEF_ExtDistThreshold())
        SetSliderDialogRange(100.0, 2000.0)
        SetSliderDialogInterval(50.0)
    ElseIf option == ExtDistDurationID
        SetSliderDialogStartValue(TETHER_ExtDistDuration.GetValue())
        SetSliderDialogDefaultValue(DEF_ExtDistDuration())
        SetSliderDialogRange(1.0, 30.0)
        SetSliderDialogInterval(0.5)

    ElseIf option == OffsetRightID
        SetSliderDialogStartValue(TETHER_OffsetRight.GetValue())
        SetSliderDialogDefaultValue(DEF_OffsetRight())
        SetSliderDialogRange(-40.0, 40.0)
        SetSliderDialogInterval(1.0)
    ElseIf option == OffsetBackID
        SetSliderDialogStartValue(TETHER_OffsetBack.GetValue())
        SetSliderDialogDefaultValue(DEF_OffsetBack())
        SetSliderDialogRange(-100.0, 0.0)
        SetSliderDialogInterval(1.0)
    ElseIf option == FollowRadiusID
        SetSliderDialogStartValue(TETHER_FollowRadius.GetValue())
        SetSliderDialogDefaultValue(DEF_FollowRadius())
        SetSliderDialogRange(5.0, 100.0)
        SetSliderDialogInterval(1.0)
    ElseIf option == CatchupRadiusID
        SetSliderDialogStartValue(TETHER_CatchupRadius.GetValue())
        SetSliderDialogDefaultValue(DEF_CatchupRadius())
        SetSliderDialogRange(10.0, 200.0)
        SetSliderDialogInterval(1.0)

    ElseIf option == TetherIdealID
        SetSliderDialogStartValue(TETHER_TetherIdealDist.GetValue())
        SetSliderDialogDefaultValue(DEF_TetherIdeal())
        SetSliderDialogRange(10.0, 200.0)
        SetSliderDialogInterval(5.0)
    ElseIf option == TetherSlackID
        SetSliderDialogStartValue(TETHER_TetherSlackDist.GetValue())
        SetSliderDialogDefaultValue(DEF_TetherSlack())
        SetSliderDialogRange(30.0, 400.0)
        SetSliderDialogInterval(5.0)
    ElseIf option == PSpeedNearID
        SetSliderDialogStartValue(TETHER_PlayerSpeedMultNear.GetValue())
        SetSliderDialogDefaultValue(DEF_PSpeedNear())
        SetSliderDialogRange(0.3, 1.0)
        SetSliderDialogInterval(0.05)
    ElseIf option == PSpeedFarID
        SetSliderDialogStartValue(TETHER_PlayerSpeedMultFar.GetValue())
        SetSliderDialogDefaultValue(DEF_PSpeedFar())
        SetSliderDialogRange(0.1, 1.0)
        SetSliderDialogInterval(0.05)
    ElseIf option == FSpeedMultID
        SetSliderDialogStartValue(TETHER_FollowerSpeedMult.GetValue())
        SetSliderDialogDefaultValue(DEF_FSpeedMult())
        SetSliderDialogRange(1.0, 2.5)
        SetSliderDialogInterval(0.05)
    ElseIf option == PalmOffsetID
        SetSliderDialogStartValue(TETHER_PalmOffset.GetValue())
        SetSliderDialogDefaultValue(DEF_PalmOffset())
        SetSliderDialogRange(0.0, 15.0)
        SetSliderDialogInterval(0.5)
    EndIf
EndEvent

; =============================================================================
;  Slider accept.
; =============================================================================
Event OnOptionSliderAccept(Int option, Float value)
    If option == ExtDistThresholdID
        SetFloat(TETHER_ExtDistThreshold, value)
        SetSliderOptionValue(option, value, "{0}u")
    ElseIf option == ExtDistDurationID
        SetFloat(TETHER_ExtDistDuration, value)
        SetSliderOptionValue(option, value, "{1}s")
    ElseIf option == OffsetRightID
        SetFloat(TETHER_OffsetRight, value)
        SetSliderOptionValue(option, value, "{0}u")
    ElseIf option == OffsetBackID
        SetFloat(TETHER_OffsetBack, value)
        SetSliderOptionValue(option, value, "{0}u")
    ElseIf option == FollowRadiusID
        SetFloat(TETHER_FollowRadius, value)
        SetSliderOptionValue(option, value, "{0}u")
    ElseIf option == CatchupRadiusID
        SetFloat(TETHER_CatchupRadius, value)
        SetSliderOptionValue(option, value, "{0}u")
    ElseIf option == TetherIdealID
        SetFloat(TETHER_TetherIdealDist, value)
        SetSliderOptionValue(option, value, "{0}u")
    ElseIf option == TetherSlackID
        SetFloat(TETHER_TetherSlackDist, value)
        SetSliderOptionValue(option, value, "{0}u")
    ElseIf option == PSpeedNearID
        SetFloat(TETHER_PlayerSpeedMultNear, value)
        SetSliderOptionValue(option, value, "{2}x")
    ElseIf option == PSpeedFarID
        SetFloat(TETHER_PlayerSpeedMultFar, value)
        SetSliderOptionValue(option, value, "{2}x")
    ElseIf option == FSpeedMultID
        SetFloat(TETHER_FollowerSpeedMult, value)
        SetSliderOptionValue(option, value, "{2}x")
    ElseIf option == PalmOffsetID
        SetFloat(TETHER_PalmOffset, value)
        SetSliderOptionValue(option, value, "{1}u")
    EndIf
EndEvent

; =============================================================================
;  Toggle / text-option select.
; =============================================================================
Event OnOptionSelect(Int option)
    If option == AutoRelWeaponID
        Bool v = !GetBool(TETHER_AutoRelWeapon)
        SetBool(TETHER_AutoRelWeapon, v)
        SetToggleOptionValue(option, v)
    ElseIf option == AutoRelCombatID
        Bool v = !GetBool(TETHER_AutoRelCombat)
        SetBool(TETHER_AutoRelCombat, v)
        SetToggleOptionValue(option, v)
    ElseIf option == AutoRelExtDistID
        Bool v = !GetBool(TETHER_AutoRelExtDist)
        SetBool(TETHER_AutoRelExtDist, v)
        SetToggleOptionValue(option, v)
        ForcePageReset()  ; enable/disable the slider rows
    ElseIf option == ResetAllID
        If ShowMessage("Reset all TETHER settings to their defaults?", True, "$Yes", "$No") == 0
            ApplyAllDefaults()
            ForcePageReset()
        EndIf
    EndIf
EndEvent

; =============================================================================
;  Keymap change (hotkey rebind — supports keyboard AND gamepad).
; =============================================================================
Event OnOptionKeyMapChange(Int option, Int keyCode, String conflictControl, String conflictName)
    If option == HotkeyID
        ; No conflict prompt: silent bind like Quick Light. If you want the warning
        ; back, uncomment the block below.
        ; If conflictControl != "" && ShowMessage("This key is used by \"" + conflictControl + "\". Bind anyway?", True) != 0
        ;     Return
        ; EndIf
        SetInt(TETHER_Hotkey, keyCode)
        SetKeyMapOptionValue(option, keyCode)
    EndIf
EndEvent

; =============================================================================
;  Per-option default (SkyUI's built-in 'R' shortcut).
; =============================================================================
Event OnOptionDefault(Int option)
    If option == HotkeyID
        SetInt(TETHER_Hotkey, DEF_Hotkey())
        SetKeyMapOptionValue(option, DEF_Hotkey())
    ElseIf option == AutoRelWeaponID
        SetBool(TETHER_AutoRelWeapon, DEF_AutoRelWeapon())
        SetToggleOptionValue(option, DEF_AutoRelWeapon())
    ElseIf option == AutoRelCombatID
        SetBool(TETHER_AutoRelCombat, DEF_AutoRelCombat())
        SetToggleOptionValue(option, DEF_AutoRelCombat())
    ElseIf option == AutoRelExtDistID
        SetBool(TETHER_AutoRelExtDist, DEF_AutoRelExtDist())
        SetToggleOptionValue(option, DEF_AutoRelExtDist())
        ForcePageReset()
    ElseIf option == ExtDistThresholdID
        SetFloat(TETHER_ExtDistThreshold, DEF_ExtDistThreshold())
        SetSliderOptionValue(option, DEF_ExtDistThreshold(), "{0}u")
    ElseIf option == ExtDistDurationID
        SetFloat(TETHER_ExtDistDuration, DEF_ExtDistDuration())
        SetSliderOptionValue(option, DEF_ExtDistDuration(), "{1}s")

    ElseIf option == OffsetRightID
        SetFloat(TETHER_OffsetRight, DEF_OffsetRight())
        SetSliderOptionValue(option, DEF_OffsetRight(), "{0}u")
    ElseIf option == OffsetBackID
        SetFloat(TETHER_OffsetBack, DEF_OffsetBack())
        SetSliderOptionValue(option, DEF_OffsetBack(), "{0}u")
    ElseIf option == FollowRadiusID
        SetFloat(TETHER_FollowRadius, DEF_FollowRadius())
        SetSliderOptionValue(option, DEF_FollowRadius(), "{0}u")
    ElseIf option == CatchupRadiusID
        SetFloat(TETHER_CatchupRadius, DEF_CatchupRadius())
        SetSliderOptionValue(option, DEF_CatchupRadius(), "{0}u")

    ElseIf option == TetherIdealID
        SetFloat(TETHER_TetherIdealDist, DEF_TetherIdeal())
        SetSliderOptionValue(option, DEF_TetherIdeal(), "{0}u")
    ElseIf option == TetherSlackID
        SetFloat(TETHER_TetherSlackDist, DEF_TetherSlack())
        SetSliderOptionValue(option, DEF_TetherSlack(), "{0}u")
    ElseIf option == PSpeedNearID
        SetFloat(TETHER_PlayerSpeedMultNear, DEF_PSpeedNear())
        SetSliderOptionValue(option, DEF_PSpeedNear(), "{2}x")
    ElseIf option == PSpeedFarID
        SetFloat(TETHER_PlayerSpeedMultFar, DEF_PSpeedFar())
        SetSliderOptionValue(option, DEF_PSpeedFar(), "{2}x")
    ElseIf option == FSpeedMultID
        SetFloat(TETHER_FollowerSpeedMult, DEF_FSpeedMult())
        SetSliderOptionValue(option, DEF_FSpeedMult(), "{2}x")
    ElseIf option == PalmOffsetID
        SetFloat(TETHER_PalmOffset, DEF_PalmOffset())
        SetSliderOptionValue(option, DEF_PalmOffset(), "{1}u")
    EndIf
EndEvent
