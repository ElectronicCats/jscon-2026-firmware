#pragma once

#include "configuration.h"

#ifdef BADGE_JSCON2026

#include "AxolotlGame.h"
#include "SinglePortModule.h"
#include "concurrency/OSThread.h"

/**
 * Radio side of the Badge JSCON 2026 axolotl game: announces this badge's axolotl to direct
 * neighbors, feeds what it hears into axolotl::Game, and keeps the collection on flash.
 * The rules live in AxolotlGame.h.
 */
class AxolotlModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    AxolotlModule();

    const axolotl::Game &game() const { return state; }

    /// Call on every draw of a game page. Sends one early hello when the page is opened. Rate limited.
    void requestBoost();

    /// The axolotl unlocked most recently, or axolotl::kCount when none this boot.
    uint8_t lastUnlocked() const { return lastUnlockedId; }

    static const char *nameOf(uint8_t id);

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual int32_t runOnce() override;

  private:
    void sendHello(bool isAnswer);
    void celebrate(uint8_t id);
    void loadFromFlash();
    void saveToFlash();

    axolotl::Game state;
    uint32_t lastHelloMs = 0;
    uint32_t helloIntervalMs = 0;
    uint32_t lastBoostMs = 0;
    uint32_t lastPageDrawMs = 0;
    uint32_t answerRequestedMs = 0;
    uint32_t answerDelayMs = 0;
    bool boostRequested = false;
    bool pageSeen = false;
    bool answerPending = false;
    uint8_t lastUnlockedId = axolotl::kCount;
};

extern AxolotlModule *axolotlModule;

#endif // BADGE_JSCON2026
