#include "BluetoothMidiService.h"
#include <deque>
#include <mutex>
#include <map>
#include <atomic>
#include <optional>
#include <exception>
#if JUCE_WINDOWS
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Storage.Streams.h>
#include <robuffer.h>
#endif

namespace fengyin
{
struct BluetoothMidiService::Impl : juce::Thread
{
    struct Device { juce::String id, name; bool seen=false, paired=false; };
    struct Command { juce::String action, id; unsigned generation; };
    BluetoothMidiService& owner;
    struct CallbackLifetime { std::mutex mutex; bool alive=true; };
    std::shared_ptr<CallbackLifetime> lifetime=std::make_shared<CallbackLifetime>();
    mutable std::mutex mutex;
    std::mutex receiverMutex;
    std::function<void(const juce::MidiMessage&)> receiver;
    std::map<juce::String, Device> devices;
    std::deque<Command> commands;
    juce::String connected, connecting, message;
    bool scanning=false, supported=false;
    std::atomic<unsigned> generation { 0 };
    juce::ApplicationProperties settings;
    double scanUntil=0;
    uint64_t portEpoch=0;
#if JUCE_WINDOWS
    using Ble = winrt::Windows::Devices::Bluetooth::BluetoothLEDevice;
    using Info = winrt::Windows::Devices::Enumeration::DeviceInformation;
    using Port = winrt::Windows::Devices::Midi::MidiInPort;
    using Watcher = winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher;
    Watcher watcher { nullptr };
    Ble ble { nullptr };
    Port port { nullptr };
    winrt::event_token receivedToken {}, connectionToken {}, stoppedToken {}, midiToken {};
    struct Advert { uint64_t address; juce::String name; bool midi=false; };
    std::deque<Advert> adverts;
    std::map<uint64_t, double> checked;
    std::atomic<bool> linkLost { false };
    std::atomic<uint64_t> transportEpoch { 0 };
    const winrt::guid midiUuid { 0x03b80e5a, 0xede8, 0x4b33, { 0xa7,0x51,0x6c,0xe3,0x4e,0xc4,0xc7,0x00 } };
    // Only the asynchronous API *start* runs on the UI thread (Windows consent
    // requirement). No .get(), pairing wait, discovery or port I/O blocks that thread.
    template<class Factory> auto beginOnUi(Factory factory, unsigned ticket)
    {
        using Operation=decltype(factory());
        struct Pending { std::mutex mutex; std::optional<Operation> operation; std::exception_ptr error; bool ready=false; std::atomic<bool> canceled{false}; };
        auto pending=std::make_shared<Pending>();
        if(!juce::MessageManager::callAsync([this,guard=lifetime,pending,ticket,factory=std::move(factory)]() mutable {
            std::lock_guard callbackLock(guard->mutex);
            std::lock_guard resultLock(pending->mutex);
            if(!guard->alive || pending->canceled || generation.load()!=ticket) pending->error=std::make_exception_ptr(winrt::hresult_canceled());
            else {try{pending->operation=factory();}catch(...){pending->error=std::current_exception();}}
            pending->ready=true;
        }))throw winrt::hresult_canceled();
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+8000;
        for(;;) {
            {std::lock_guard lock(pending->mutex);if(pending->ready){if(pending->error)std::rethrow_exception(pending->error);return *pending->operation;}}
            if(threadShouldExit() || generation.load()!=ticket || juce::Time::getMillisecondCounterHiRes()>deadline) {
                pending->canceled=true;throw winrt::hresult_canceled();
            }
            wait(20);
        }
    }
    template<class T> auto await(T operation, unsigned ticket, int ms=8000)
    {
        const auto end=juce::Time::getMillisecondCounterHiRes()+ms;
        while (operation.Status()==winrt::Windows::Foundation::AsyncStatus::Started)
        {
            if (threadShouldExit() || generation.load()!=ticket || juce::Time::getMillisecondCounterHiRes()>end)
            { operation.Cancel(); throw winrt::hresult_canceled(); }
            wait(20);
        }
        return operation.GetResults();
    }
    Info deviceInfo(const winrt::hstring& id, const winrt::Windows::Foundation::Collections::IVector<winrt::hstring>& props, unsigned ticket)
    {
        Info info { nullptr };
        // BluetoothLEDevice.DeviceId normally denotes a device interface. An AEP
        // selector returns a different identity kind; neither may be guessed by name.
        try {info=await(Info::CreateFromIdAsync(id,props),ticket);}
        catch(const winrt::hresult_error& error) {if(error.code()==E_ACCESSDENIED || error.code()==HRESULT_FROM_WIN32(ERROR_CANCELLED))throw;}
        if(!info) {
            try {info=await(Info::CreateFromIdAsync(id,props,winrt::Windows::Devices::Enumeration::DeviceInformationKind::AssociationEndpoint),ticket);}
            catch(const winrt::hresult_error& error) {
                const auto code=error.code();
                if(code!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && code!=HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
                    && code!=HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) && code!=E_INVALIDARG)throw;
            }
        }
        return info;
    }
    static juce::String str(const winrt::hstring& value) { return juce::String(value.c_str()); }
    static winrt::hstring wide(const juce::String& value) { return winrt::hstring(value.toWideCharPointer()); }
#endif
    explicit Impl(BluetoothMidiService& parent, bool persistSettings) : juce::Thread("Bluetooth MIDI control"), owner(parent)
    {
        juce::PropertiesFile::Options options;
        options.applicationName="FengYinBluetoothMidi"; options.filenameSuffix=".settings";
        options.folderName="FengYin"; options.osxLibrarySubFolder="Application Support";
        if(persistSettings) settings.setStorageParameters(options);
        juce::var savedHistory;
        if(auto* file=settings.getUserSettings()) savedHistory=juce::JSON::parse(file->getValue("history"));
        if(auto* rows=savedHistory.getArray())
            for(const auto& row:*rows) {
                Device d { row["id"].toString(), row["name"].toString(), false, false };
                if(d.id.isNotEmpty()) devices[d.id]=d;
            }
        startThread();
    }
    ~Impl() override { signalThreadShouldExit(); notify(); stopThread(-1); }
    void changed() { owner.triggerAsyncUpdate(); }
    void save()
    {
        juce::Array<juce::var> rows;
        for(const auto& [id,d]:devices) if(history.count(id)) {
            auto* row=new juce::DynamicObject(); row->setProperty("id",id);row->setProperty("name",d.name);rows.add(juce::var(row));
        }
        if(auto* file=settings.getUserSettings()) {file->setValue("history",juce::JSON::toString(juce::var(rows)));file->saveIfNeeded();}
    }
    std::map<juce::String,bool> history;
    void push(const juce::String& action,const juce::String& id)
    {
        const bool cancels=action=="connect" || action=="cancel" || action=="disconnect" || action=="forget";
        std::lock_guard lock(mutex);
        if((action=="connect" || action=="forget") && !devices.count(id)) return;
        if(cancels) ++generation;
        commands.push_back({action,id,generation.load()}); notify();
    }
    void clearReceiver() { std::lock_guard lock(receiverMutex); receiver={}; }
#if JUCE_WINDOWS
    void closePort()
    {
        ++transportEpoch;
        clearReceiver();
        if(port) {try{port.MessageReceived(midiToken);port.Close();}catch(...){}port=nullptr;}
        if(ble) {try{ble.ConnectionStatusChanged(connectionToken);ble.Close();}catch(...){}ble=nullptr;}
        linkLost=false;
        std::lock_guard lock(mutex);if(devices.count(connected))devices[connected].seen=false;connected.clear();
    }
    void stopScan()
    {
        if(watcher) {try{watcher.Received(receivedToken);watcher.Stopped(stoppedToken);watcher.Stop();}catch(...){}watcher=nullptr;}
        std::lock_guard lock(mutex); scanning=false; adverts.clear();
    }
    void startScan(unsigned ticket)
    {
        using namespace winrt::Windows::Devices;
        stopScan();
        bool radioExists=false, radioOn=false;
        for(const auto& radio:await(Radios::Radio::GetRadiosAsync(),ticket))
            if(radio.Kind()==Radios::RadioKind::Bluetooth) {radioExists=true;radioOn|=radio.State()==Radios::RadioState::On;}
        if(!radioOn) {std::lock_guard lock(mutex);message=radioExists?juce::String::fromUTF8("请开启电脑蓝牙后重新搜索"):juce::String::fromUTF8("未找到蓝牙适配器，可使用 USB 连接");changed();return;}
        {
            std::lock_guard lock(mutex);for(auto it=devices.begin();it!=devices.end();) {
                if(!history.count(it->first) && it->first!=connected) it=devices.erase(it);
                else {it->second.seen=(it->first==connected);++it;}
            }
            scanning=true;message=juce::String::fromUTF8("正在搜索蓝牙 MIDI 电吹管，请开启设备蓝牙并靠近电脑");
        }
        checked.clear();scanUntil=juce::Time::getMillisecondCounterHiRes()+20000;
        watcher=Watcher();watcher.ScanningMode(Bluetooth::Advertisement::BluetoothLEScanningMode::Active);
        receivedToken=watcher.Received([this, guard=lifetime](const auto&,const Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs& args) {
            std::lock_guard callbackLock(guard->mutex);if(!guard->alive)return;
            bool midi=false;for(const auto& uuid:args.Advertisement().ServiceUuids())midi|=uuid==midiUuid;
            std::lock_guard lock(mutex);
            if(scanning && adverts.size()<128)adverts.push_back({args.BluetoothAddress(),str(args.Advertisement().LocalName()),midi});
            notify();
        });
        stoppedToken=watcher.Stopped([this, guard=lifetime](const auto&,const Bluetooth::Advertisement::BluetoothLEAdvertisementWatcherStoppedEventArgs& args) {
            std::lock_guard callbackLock(guard->mutex);if(!guard->alive)return;
            if(args.Error()!=Bluetooth::BluetoothError::Success) {
                std::lock_guard lock(mutex);scanning=false;message=juce::String::fromUTF8("蓝牙搜索已停止，请检查蓝牙权限或开关后重试");changed();
            }
        });
        watcher.Start();changed();
        // A previously paired controller may already be physically connected and
        // no longer advertise. Do not confuse a stale pairing with its presence.
        const auto paired=await(Info::FindAllAsync(Ble::GetDeviceSelectorFromPairingState(true)),ticket);
        for(const auto& info:paired) {
            if(generation.load()!=ticket || threadShouldExit())break;
            auto device=await(beginOnUi([id=info.Id()]{return Ble::FromIdAsync(id);},ticket),ticket,1800);
            if(!device)continue;
            const auto address=device.BluetoothAddress();
            const auto present=device.ConnectionStatus()==Bluetooth::BluetoothConnectionStatus::Connected;
            const auto name=str(device.Name());device.Close();
            if(present) {try {inspectAdvert({address,name,false},ticket);}catch(...) {}}
        }

    }
    void inspectAdvert(const Advert& advert, unsigned ticket)
    {
        const auto now=juce::Time::getMillisecondCounterHiRes();
        if(checked.count(advert.address) && now-checked[advert.address]<2500)return;
        checked[advert.address]=now;
        auto device=await(beginOnUi([address=advert.address]{return Ble::FromBluetoothAddressAsync(address);},ticket),ticket,1800);
        if(!device)return;
        bool isMidi=advert.midi;
        if(!isMidi) {
            auto services=await(device.GetGattServicesForUuidAsync(midiUuid,
                winrt::Windows::Devices::Bluetooth::BluetoothCacheMode::Cached),ticket,1800);
            isMidi=services.Status()==winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus::Success && services.Services().Size()>0;
            for(auto service:services.Services())service.Close();
        }
        if(isMidi) {
            Device d {str(device.DeviceId()),str(device.Name()),true,device.DeviceInformation().Pairing().IsPaired()};
            if(d.name.isEmpty())d.name=advert.name.isNotEmpty()?advert.name:juce::String::fromUTF8("蓝牙 MIDI 电吹管");
            {std::lock_guard lock(mutex);devices[d.id]=d;}changed();
        }
        device.Close();
    }
    void open(const Command& command)
    {
        using namespace winrt::Windows::Devices;
        stopScan();
        closePort(); // Release a previous BLE input before reopening; USB is managed independently.
        {std::lock_guard lock(mutex);connecting=command.id;message=juce::String::fromUTF8("正在连接，首次使用时请确认系统配对提示");}changed();
        auto candidate=await(beginOnUi([id=wide(command.id)]{return Ble::FromIdAsync(id);},command.generation),command.generation);
        if(!candidate)throw winrt::hresult_error(E_ACCESSDENIED);
        auto info=candidate.DeviceInformation();
        if(!info.Pairing().IsPaired()) {
            auto custom=info.Pairing().Custom();
            auto token=custom.PairingRequested([](const auto&,const Enumeration::DevicePairingRequestedEventArgs& args) {
                if(args.PairingKind()==Enumeration::DevicePairingKinds::ConfirmOnly)args.Accept();
            });
            Enumeration::DevicePairingResult result { nullptr };
            try {result=await(beginOnUi([custom]{return custom.PairAsync(Enumeration::DevicePairingKinds::ConfirmOnly,
                       Enumeration::DevicePairingProtectionLevel::None);},command.generation),command.generation,45000);}
            catch(...) {custom.PairingRequested(token);throw;}
            custom.PairingRequested(token);
            if(result.Status()!=Enumeration::DevicePairingResultStatus::Paired && result.Status()!=Enumeration::DevicePairingResultStatus::AlreadyPaired)
                throw winrt::hresult_error(E_FAIL,L"Pairing did not complete");
        }
        // Endpoint identity must match the Bluetooth container. Never open an unrelated USB port by name.
        const auto props=winrt::single_threaded_vector<winrt::hstring>({L"System.Devices.ContainerId"});
        const auto source=deviceInfo(info.Id(),props,command.generation);
        if(!source)throw winrt::hresult_error(E_FAIL,L"Bluetooth device identity unavailable");
        const auto container=source.Properties().TryLookup(L"System.Devices.ContainerId");
        Port opened { nullptr };
        for(int attempt=0;attempt<16 && !opened;++attempt) {
            auto endpoints=await(Info::FindAllAsync(Port::GetDeviceSelector(),props),command.generation);
            for(const auto& endpoint:endpoints) {
                auto target=endpoint.Properties().TryLookup(L"System.Devices.ContainerId");
                if(container && target && winrt::unbox_value<winrt::guid>(container)==winrt::unbox_value<winrt::guid>(target)) {
                    opened=await(Port::FromIdAsync(endpoint.Id()),command.generation);
                    if(opened)break;
                }
            }
            if(!opened) {if(generation.load()!=command.generation)throw winrt::hresult_canceled();wait(300);}
        }
        if(!opened)throw winrt::hresult_error(E_FAIL,L"Bluetooth MIDI input port is not available");
        if(generation.load()!=command.generation){opened.Close();throw winrt::hresult_canceled();}
        ble=candidate;port=opened;
        const auto epoch=transportEpoch.load();
        try {
        midiToken=port.MessageReceived([this, guard=lifetime,epoch](const auto&,const Midi::MidiMessageReceivedEventArgs& args) {
            std::lock_guard callbackLock(guard->mutex);if(!guard->alive)return;
            if(transportEpoch.load()!=epoch)return;
            try {
            auto buffer=args.Message().RawData();
            auto access=buffer.as<::Windows::Storage::Streams::IBufferByteAccess>();
            byte* bytes=nullptr;if(FAILED(access->Buffer(&bytes)) || !bytes)return;
            // Continuous MIDI bypasses UI, polling, disk, and the discovery worker.
            std::lock_guard lock(receiverMutex);
            if(receiver && buffer.Length()>0 && buffer.Length()<=3)
                receiver(juce::MidiMessage(bytes,static_cast<int>(buffer.Length()),juce::Time::getMillisecondCounterHiRes()*0.001));
            } catch(...) {linkLost=true;notify();}
        });
        connectionToken=ble.ConnectionStatusChanged([this, guard=lifetime,epoch](const auto& device,const auto&) {
            std::lock_guard callbackLock(guard->mutex);if(!guard->alive)return;
            if(transportEpoch.load()!=epoch)return;
            try {if(device.ConnectionStatus()==Bluetooth::BluetoothConnectionStatus::Disconnected){linkLost=true;notify();}}catch(...) {linkLost=true;notify();}
        });
        {std::lock_guard lock(mutex);connected=command.id;++portEpoch;connecting.clear();history[command.id]=true;
            devices[command.id].paired=true;devices[command.id].seen=true;save();message=juce::String::fromUTF8("蓝牙 MIDI 已连接");}changed();
        } catch(...) {closePort();throw;}
    }
#endif
    void run() override
    {
        {std::lock_guard lock(mutex);for(const auto& [id,d]:devices)history[id]=true;}
#if JUCE_WINDOWS
        using namespace winrt::Windows::Devices;
        try {winrt::init_apartment(winrt::apartment_type::multi_threaded);std::lock_guard lock(mutex);supported=true;}
        catch(...) {std::lock_guard lock(mutex);message=juce::String::fromUTF8("蓝牙 MIDI 初始化失败，请使用 USB 连接");changed();return;}
        while(!threadShouldExit()) {
            Command command;bool hasCommand=false;
            {std::lock_guard lock(mutex);if(!commands.empty()){command=commands.front();commands.pop_front();hasCommand=true;}}
            try {
                if(hasCommand) {
                    if(command.action=="scan")startScan(command.generation);
                    else if(command.action=="stopScan")stopScan();
                    else if(command.action=="connect" && command.generation==generation.load())open(command);
                    else if(command.action=="disconnect" || command.action=="cancel") {
                        closePort();std::lock_guard lock(mutex);connecting.clear();message=juce::String::fromUTF8("已断开，可选择设备重新连接");
                    }
                    else if(command.action=="forget") {
                        bool active;{std::lock_guard lock(mutex);active=connected==command.id;}if(active)closePort();
                        auto info=deviceInfo(wide(command.id),winrt::single_threaded_vector<winrt::hstring>(),command.generation);
                        if(info && info.Pairing().IsPaired()) {
                            const auto result=await(info.Pairing().UnpairAsync(),command.generation);
                            if(result.Status()!=Enumeration::DeviceUnpairingResultStatus::Unpaired && result.Status()!=Enumeration::DeviceUnpairingResultStatus::AlreadyUnpaired)
                                throw winrt::hresult_error(E_FAIL,L"Unable to unpair device");
                        }
                        std::lock_guard lock(mutex);history.erase(command.id);devices.erase(command.id);save();message=juce::String::fromUTF8("已忘记设备");
                    }
                    changed();
                }
                if(linkLost.exchange(false)) {closePort();std::lock_guard lock(mutex);message=juce::String::fromUTF8("蓝牙连接已断开，请唤醒电吹管后重新连接");changed();}
                Advert advert{};bool inspect=false;
                {std::lock_guard lock(mutex);if(scanning && !adverts.empty()){advert=adverts.front();adverts.pop_front();inspect=true;}}
                if(inspect) {try{inspectAdvert(advert,generation.load());}catch(...) { /* A non-MIDI advertiser need not grant GATT access. */ }}
                if(scanUntil>0 && juce::Time::getMillisecondCounterHiRes()>=scanUntil) {
                    scanUntil=0;stopScan();std::lock_guard lock(mutex);message=juce::String::fromUTF8("搜索完成。未发现设备时，请确认电吹管已开启蓝牙且未被其他应用占用");changed();
                }
            } catch(const winrt::hresult_error& error) {
                std::lock_guard lock(mutex);connecting.clear();
                message=error.code()==E_ACCESSDENIED?juce::String::fromUTF8("蓝牙访问被拒绝，请检查权限后重试")
                    :error.code()==HRESULT_FROM_WIN32(ERROR_CANCELLED)?juce::String::fromUTF8("连接已取消或超时，请重试")
                    :juce::String::fromUTF8("连接未完成，请检查电吹管蓝牙与配对状态后重试");
                juce::Logger::writeToLog("Bluetooth MIDI: "+juce::String::toHexString(static_cast<int>(error.code()))+" "+str(error.message()));changed();
            } catch(...) {std::lock_guard lock(mutex);connecting.clear();message=juce::String::fromUTF8("蓝牙操作未完成，请重试或使用 USB 连接");changed();}
            wait(250);
        }
        stopScan();closePort();winrt::uninit_apartment();
#else
        std::lock_guard lock(mutex);message=juce::String::fromUTF8("此版本的蓝牙 MIDI 连接支持 Windows，当前可使用 USB 连接");changed();
#endif
    }
};
BluetoothMidiService::BluetoothMidiService(bool persistSettings) : impl(std::make_unique<Impl>(*this,persistSettings))
{
#if JUCE_WINDOWS
    try {winrt::init_apartment(winrt::apartment_type::single_threaded);uiApartmentInitialised=true;}
    catch(const winrt::hresult_error&) { /* An existing MTA apartment remains valid. */ }
#endif
}
BluetoothMidiService::~BluetoothMidiService()
{
    shutdown();
#if JUCE_WINDOWS
    if(uiApartmentInitialised)winrt::uninit_apartment();
#endif
}
void BluetoothMidiService::shutdown() { if(impl){{std::lock_guard lock(impl->lifetime->mutex);impl->lifetime->alive=false;}impl->generation++;impl->clearReceiver();impl.reset();}cancelPendingUpdate(); }
void BluetoothMidiService::command(const juce::String& action,const juce::String& id) { if(impl)impl->push(action,id); }
void BluetoothMidiService::setReceiver(std::function<void(const juce::MidiMessage&)> receiver)
{if(impl){std::lock_guard lock(impl->receiverMutex);impl->receiver=std::move(receiver);}}
void BluetoothMidiService::handleAsyncUpdate() { if(onChange)onChange(); }
juce::var BluetoothMidiService::snapshot() const
{
    auto* state=new juce::DynamicObject();if(!impl)return juce::var(state);
    std::lock_guard lock(impl->mutex);
    state->setProperty("portEpoch",static_cast<juce::int64>(impl->portEpoch));
    state->setProperty("supported",impl->supported);state->setProperty("scanning",impl->scanning);
    state->setProperty("connectedId",impl->connected);state->setProperty("connectingId",impl->connecting);state->setProperty("message",impl->message);
    juce::Array<juce::var> rows;
    for(const auto& [id,d]:impl->devices) {auto* row=new juce::DynamicObject();row->setProperty("id",id);row->setProperty("name",d.name);
        row->setProperty("available",d.seen);row->setProperty("remembered",impl->history.count(id)>0);row->setProperty("paired",d.paired);rows.add(juce::var(row));}
    state->setProperty("devices",juce::var(rows));return juce::var(state);
}
}
