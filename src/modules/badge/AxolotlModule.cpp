#include "AxolotlModule.h"

#ifdef BADGE_JSCON2026

#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "SPILock.h"
#include "Throttle.h"
#include "airtime.h"
#include "graphics/Screen.h"
#include "main.h"
#include "target_specific.h"

AxolotlModule *axolotlModule;

namespace
{
const char *const saveFileName = "/prefs/axolotl";

constexpr uint32_t kTickMs = 500;
// 100 badges on SHORT_FAST: a 40 ms hello every 55 s on average keeps the game near 7% of the channel.
constexpr uint32_t kHelloMinMs = 40000;
constexpr uint32_t kHelloMaxMs = 70000;
constexpr uint32_t kFirstHelloMs = 8000;
constexpr uint32_t kBoostMinGapMs = 10000;
constexpr uint32_t kAnswerMaxDelayMs = 2000;
constexpr uint32_t kCelebrateMs = 5000;

const char *const kNames[axolotl::kCount] = {"Agente", "Burbujas", "Elote", "Abanico", "Finanzas", "Feliz",
                                             "Poncho", "Arbitro",  "Scout", "Sede",    "Vibras"};
} // namespace

const char *AxolotlModule::nameOf(uint8_t id)
{
    return id < axolotl::kCount ? kNames[id] : "?";
}

AxolotlModule::AxolotlModule() : SinglePortModule("axolotl", meshtastic_PortNum_PRIVATE_APP), concurrency::OSThread("Axolotl")
{
    // Seeded from the MAC address, not the node number: the node number is a hash of the public
    // key and changes whenever the keys are regenerated, the MAC address never does.
    uint8_t mac[6];
    getMacAddr(mac);
    state.begin((uint32_t)mac[2] << 24 | (uint32_t)mac[3] << 16 | (uint32_t)mac[4] << 8 | mac[5]);
    loadFromFlash();
#ifdef BADGE_AXOLOTL_PREVIEW_ALL
    state.previewAllCollected();
    LOG_WARN("Axolotl: PREVIEW build, every axolotl shown as collected");
#endif
    helloIntervalMs = kFirstHelloMs + random(kFirstHelloMs);
    lastHelloMs = millis();
    LOG_INFO("Axolotl: own=%s (%u) collected=%u/%u met=%u", nameOf(state.home()), state.home(), state.collectedCount(),
             axolotl::kCount, state.peopleMet());
    setIntervalFromNow(kTickMs);
}

void AxolotlModule::requestBoost()
{
    boostRequested = true;
}

ProcessMessage AxolotlModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    axolotl::Hello hello;
    if (isFromUs(&mp) || !decodeHello(mp.decoded.payload.bytes, mp.decoded.payload.size, hello))
        return ProcessMessage::CONTINUE; // some other use of the private port

    const bool direct = getHopsAway(mp) == 0 && !mp.via_mqtt && mp.has_rx_rssi;
    LOG_INFO("Axolotl: heard 0x%08x %s%s rssi=%d snr=%.1f direct=%d", mp.from, nameOf(hello.home),
             hello.isAnswer ? " (answer)" : "", (int)mp.rx_rssi, mp.rx_snr, direct);

    const axolotl::Outcome outcome = state.onHello(mp.from, hello.home, mp.rx_rssi, direct, millis());
    if (outcome.unlocked)
        celebrate(outcome.axolotl);

    if (!answerPending && state.shouldAnswer(hello, mp.rx_rssi, direct)) {
        answerPending = true;
        answerRequestedMs = millis();
        answerDelayMs = random(kAnswerMaxDelayMs);
    }
    return ProcessMessage::CONTINUE;
}

int32_t AxolotlModule::runOnce()
{
    if (answerPending && Throttle::hasElapsed(answerRequestedMs, answerDelayMs)) {
        answerPending = false;
        sendHello(true);
    }

    const bool boost = boostRequested && Throttle::hasElapsed(lastBoostMs, kBoostMinGapMs);
    if (boost || Throttle::hasElapsed(lastHelloMs, helloIntervalMs)) {
        if (boost)
            lastBoostMs = millis();
        boostRequested = false;
        sendHello(false);
    }

    // Saved on the first tick after a change: the badge has no battery, so an unlock must reach
    // the flash before someone unplugs it.
    if (state.isDirty())
        saveToFlash();
    return kTickMs;
}

void AxolotlModule::sendHello(bool isAnswer)
{
    if (!isAnswer) {
        lastHelloMs = millis();
        helloIntervalMs = kHelloMinMs + random(kHelloMaxMs - kHelloMinMs);
    }
    if (!airTime->isTxAllowedChannelUtil(true)) {
        LOG_DEBUG("Axolotl: channel busy, hello skipped");
        return;
    }
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p)
        return;
    // Broadcast, never relayed: a hello is only meaningful to badges that hear it first hand.
    p->to = NODENUM_BROADCAST;
    p->hop_limit = 0;
    p->want_ack = false;
    p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;
    p->decoded.payload.size = encodeHello(state.makeHello(isAnswer), p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes));
    service->sendToMesh(p);
}

void AxolotlModule::celebrate(uint8_t id)
{
    lastUnlockedId = id;
    LOG_INFO("Axolotl: unlocked %s, now %u/%u", nameOf(id), state.collectedCount(), axolotl::kCount);
#if HAS_SCREEN
    if (!screen)
        return;
    static char message[64];
    if (state.isComplete())
        snprintf(message, sizeof(message), "Maestro Ajolotero!\n%u de %u", axolotl::kCount, axolotl::kCount);
    else
        snprintf(message, sizeof(message), "Nuevo ajolote!\n%s  (%u de %u)", nameOf(id), state.collectedCount(), axolotl::kCount);
    graphics::BannerOverlayOptions options;
    options.message = message;
    options.durationMs = kCelebrateMs;
    screen->showOverlayBanner(options);
#endif
}

void AxolotlModule::loadFromFlash()
{
#ifdef FSCom
    uint8_t blob[axolotl::kSaveSize];
    size_t got = 0;
    spiLock->lock();
    auto file = FSCom.open(saveFileName, FILE_O_READ);
    if (file) {
        got = file.read(blob, sizeof(blob));
        file.close();
    }
    spiLock->unlock();
    if (got == 0)
        LOG_INFO("Axolotl: no saved collection, starting fresh");
    else if (!state.load(blob, got))
        LOG_WARN("Axolotl: saved collection is damaged, starting fresh");
#endif
}

void AxolotlModule::saveToFlash()
{
#ifdef FSCom
    uint8_t blob[axolotl::kSaveSize];
    const size_t size = state.save(blob, sizeof(blob));
    bool ok = false;
    spiLock->lock();
    FSCom.mkdir("/prefs");
    if (FSCom.exists(saveFileName))
        FSCom.remove(saveFileName);
    auto file = FSCom.open(saveFileName, FILE_O_WRITE);
    if (file) {
        ok = file.write(blob, size) == size;
        file.flush();
        file.close();
    }
    spiLock->unlock();
    if (ok)
        LOG_INFO("Axolotl: saved collection, %u/%u", state.collectedCount(), axolotl::kCount);
    else
        LOG_WARN("Axolotl: could not write %s", saveFileName);
#endif
    // Cleared even on failure: retrying every tick would wear the flash for no gain.
    state.clearDirty();
}

#endif // BADGE_JSCON2026
