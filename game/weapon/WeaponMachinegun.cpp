#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "../Weapon.h"

class rvWeaponMachinegun : public rvWeapon {
public:

    CLASS_PROTOTYPE(rvWeaponMachinegun);

    rvWeaponMachinegun(void);

    virtual void        Spawn(void);
    virtual void        Think(void);
    void                Save(idSaveGame* savefile) const;
    void                Restore(idRestoreGame* savefile);
    void                PreSave(void);
    void                PostSave(void);

protected:

    float                spreadZoom;
    bool                fireHeld;

    bool                UpdateFlashlight(void);
    void                Flashlight(bool on);

private:

    float                heatLevel;
    bool                isOverheated;
    int                    overheatEndTime;

    stateResult_t        State_Idle(const stateParms_t& parms);
    stateResult_t        State_Fire(const stateParms_t& parms);
    stateResult_t        State_Reload(const stateParms_t& parms);
    stateResult_t        State_Flashlight(const stateParms_t& parms);

    CLASS_STATES_PROTOTYPE(rvWeaponMachinegun);
};

CLASS_DECLARATION(rvWeapon, rvWeaponMachinegun)
END_CLASS

rvWeaponMachinegun::rvWeaponMachinegun(void) {
    heatLevel = 0.0f;
    isOverheated = false;
    overheatEndTime = 0;
}

void rvWeaponMachinegun::Spawn(void) {
    spreadZoom = spawnArgs.GetFloat("spreadZoom");
    fireHeld = false;
    isOverheated = false;
    overheatEndTime = 0;

    SetState("Raise", 0);
    Flashlight(owner->IsFlashlightOn());
}

void rvWeaponMachinegun::Save(idSaveGame* savefile) const {
    savefile->WriteFloat(spreadZoom);
    savefile->WriteBool(fireHeld);
    savefile->WriteFloat(heatLevel);
    savefile->WriteBool(isOverheated);
    savefile->WriteInt(overheatEndTime);
}

void rvWeaponMachinegun::Restore(idRestoreGame* savefile) {
    savefile->ReadFloat(spreadZoom);
    savefile->ReadBool(fireHeld);
    savefile->ReadFloat(heatLevel);
    savefile->ReadBool(isOverheated);
    savefile->ReadInt(overheatEndTime);
}

void rvWeaponMachinegun::PreSave(void) {
}

void rvWeaponMachinegun::PostSave(void) {
}

void rvWeaponMachinegun::Think() {
    rvWeapon::Think();
    if (zoomGui && owner == gameLocal.GetLocalPlayer()) {
        zoomGui->SetStateFloat("playerYaw", playerViewAxis.ToAngles().yaw);
    }
}

bool rvWeaponMachinegun::UpdateFlashlight(void) {
    if (!wsfl.flashlight) {
        return false;
    }
    SetState("Flashlight", 0);
    return true;
}

void rvWeaponMachinegun::Flashlight(bool on) {
    owner->Flashlight(on);
    if (on) {
        viewModel->ShowSurface("models/weapons/blaster/flare");
        worldModel->ShowSurface("models/weapons/blaster/flare");
    }
    else {
        viewModel->HideSurface("models/weapons/blaster/flare");
        worldModel->HideSurface("models/weapons/blaster/flare");
    }
}

CLASS_STATES_DECLARATION(rvWeaponMachinegun)
STATE("Idle", rvWeaponMachinegun::State_Idle)
STATE("Fire", rvWeaponMachinegun::State_Fire)
STATE("Reload", rvWeaponMachinegun::State_Reload)
STATE("Flashlight", rvWeaponMachinegun::State_Flashlight)
END_CLASS_STATES

stateResult_t rvWeaponMachinegun::State_Idle(const stateParms_t& parms) {
    enum {
        STAGE_INIT,
        STAGE_WAIT,
    };
    switch (parms.stage) {
    case STAGE_INIT:
        if (!AmmoAvailable()) {
            SetStatus(WP_OUTOFAMMO);
        }
        else {
            SetStatus(WP_READY);
        }
        PlayCycle(ANIMCHANNEL_ALL, "idle", parms.blendFrames);
        return SRESULT_STAGE(STAGE_WAIT);

    case STAGE_WAIT:
        if (wsfl.lowerWeapon) {
            SetState("Lower", 4);
            return SRESULT_DONE;
        }
        if (UpdateFlashlight()) {
            return SRESULT_DONE;
        }

        if (fireHeld && !wsfl.attack) {
            fireHeld = false;
        }

        
        if (isOverheated && gameLocal.time >= overheatEndTime) {
            isOverheated = false;
            heatLevel = 0.0f;
            gameLocal.Printf("Machinegun cooled down.\n");
        }

        if (!wsfl.attack && heatLevel > 0.0f) {
            heatLevel -= 10.0f;
            if (heatLevel < 0.0f) {
                heatLevel = 0.0f;
            }
        }

        if (!clipSize) {
            if (!fireHeld && gameLocal.time > nextAttackTime && wsfl.attack && AmmoAvailable()) {
                SetState("Fire", 0);
                return SRESULT_DONE;
            }
        }
        else {
            if (!fireHeld && gameLocal.time > nextAttackTime && wsfl.attack && AmmoInClip()) {
                SetState("Fire", 0);
                return SRESULT_DONE;
            }
            if (wsfl.attack && AutoReload() && !AmmoInClip() && AmmoAvailable()) {
                SetState("Reload", 4);
                return SRESULT_DONE;
            }
            if (wsfl.netReload || (wsfl.reload && AmmoInClip() < ClipSize() && AmmoAvailable() > AmmoInClip())) {
                SetState("Reload", 4);
                return SRESULT_DONE;
            }
        }
        return SRESULT_WAIT;
    }
    return SRESULT_ERROR;
}

stateResult_t rvWeaponMachinegun::State_Fire(const stateParms_t& parms) {
    enum {
        STAGE_INIT,
        STAGE_WAIT,
    };
    switch (parms.stage) {
    case STAGE_INIT:

        // Overheat check
        if (isOverheated) {
            if (gameLocal.time >= overheatEndTime) {
                isOverheated = false;
                heatLevel = 0.0f;
            }
            else {
                gameLocal.Printf("Machinegun is overheated!\n");
                return SRESULT_DONE;
            }
        }

        // Fire logic
        if (wsfl.zoom) {
            nextAttackTime = gameLocal.time + (altFireRate * owner->PowerUpModifier(PMOD_FIRERATE));
            Attack(true, 1, spread * 2.5f, 0, 1.0f);
        }
        else {
            nextAttackTime = gameLocal.time + (fireRate * owner->PowerUpModifier(PMOD_FIRERATE));
            Attack(false, 1, spread, 0, 1.0f);
        }
        PlayAnim(ANIMCHANNEL_ALL, "fire", 0);

        // Increase heat
        heatLevel += 5.0f;
        if (heatLevel >= 100.0f) {
            isOverheated = true;
            overheatEndTime = gameLocal.time + 3000;
            gameLocal.Printf("Machinegun overheated!\n");
            return SRESULT_DONE;
        }

        return SRESULT_STAGE(STAGE_WAIT);

    case STAGE_WAIT:
        if (!fireHeld && wsfl.attack && gameLocal.time >= nextAttackTime && AmmoInClip() && !wsfl.lowerWeapon) {
            SetState("Fire", 0);
            return SRESULT_DONE;
        }
        if (AnimDone(ANIMCHANNEL_ALL, 0)) {
            SetState("Idle", 0);
            return SRESULT_DONE;
        }
        if (UpdateFlashlight()) {
            return SRESULT_DONE;
        }
        return SRESULT_WAIT;
    }
    return SRESULT_ERROR;
}

stateResult_t rvWeaponMachinegun::State_Reload(const stateParms_t& parms) {
    enum {
        STAGE_INIT,
        STAGE_WAIT,
    };
    switch (parms.stage) {
    case STAGE_INIT:
        if (wsfl.netReload) {
            wsfl.netReload = false;
        }
        else {
            NetReload();
        }

        SetStatus(WP_RELOAD);
        PlayAnim(ANIMCHANNEL_ALL, "reload", parms.blendFrames);
        return SRESULT_STAGE(STAGE_WAIT);

    case STAGE_WAIT:
        if (AnimDone(ANIMCHANNEL_ALL, 4)) {
            AddToClip(ClipSize());
            SetState("Idle", 4);
            return SRESULT_DONE;
        }
        if (wsfl.lowerWeapon) {
            SetState("Lower", 4);
            return SRESULT_DONE;
        }
        return SRESULT_WAIT;
    }
    return SRESULT_ERROR;
}

stateResult_t rvWeaponMachinegun::State_Flashlight(const stateParms_t& parms) {
    enum {
        FLASHLIGHT_INIT,
        FLASHLIGHT_WAIT,
    };
    switch (parms.stage) {
    case FLASHLIGHT_INIT:
        SetStatus(WP_FLASHLIGHT);
        PlayAnim(ANIMCHANNEL_ALL, "flashlight", 0);
        return SRESULT_STAGE(FLASHLIGHT_WAIT);

    case FLASHLIGHT_WAIT:
        if (!AnimDone(ANIMCHANNEL_ALL, 4)) {
            return SRESULT_WAIT;
        }

        if (owner->IsFlashlightOn()) {
            Flashlight(false);
        }
        else {
            Flashlight(true);
        }

        SetState("Idle", 4);
        return SRESULT_DONE;
    }
    return SRESULT_ERROR;
}
