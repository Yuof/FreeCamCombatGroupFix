// Fixes the crashes at SkyrimSE+0CC90CF, +083D0CD and in VCRUNTIME140's memmove under the player's combat-group update
// (1.6.1170 offsets) that happen while the free camera is on (tfc, or a mod's photo / preview camera).
// The player's combat-group upkeep (join or leave a combat group, the time-in-group counter, the combat flags) is run
// every frame by the combat manager job on a worker thread, in every camera mode. While the camera is the free camera,
// Main's player step also runs a reduced player update of its own, and that update calls the same upkeep on the main
// thread. Neither caller takes a lock: the two threads remove the player from one group at once (the member count
// wraps, or a copy reads past the end) or one clears the group pointer the other has just stored and reads back.
// The main thread's call is replaced by a 5-byte nop, so the free camera works like every other camera: the job's
// call stays, and at load the upkeep still runs through its own call there. The call is found by its target inside
// the free-camera update (Address Library ids for both); any other code there leaves the game unpatched.

namespace
{
	const REL::RelocationID kFreeCameraUpdate{ 39378, 40450 };  // the player update Main runs only in free camera
	const REL::RelocationID kGroupUpkeep{ 40235, 41238 };       // the player's combat-group upkeep
	constexpr std::size_t   kSpan = 0x300;                      // the free-camera update's size is 0x200-0x220
	constexpr std::array<std::uint8_t, 5> kNop5{ 0x0F, 0x1F, 0x44, 0x00, 0x00 };

	// the one `call kGroupUpkeep` inside the free-camera update, or why there is none
	std::uintptr_t FindCall(std::string_view& a_why)
	{
		const auto start = kFreeCameraUpdate.address();
		const auto target = kGroupUpkeep.address();
		const auto* code = reinterpret_cast<const std::uint8_t*>(start);
		std::uintptr_t site = 0;
		int calls = 0, nops = 0;
		for (std::size_t i = 0; i + 5 <= kSpan; ++i) {
			const auto here = start + i;
			if (code[i] == 0xE8 && here + 5 + *reinterpret_cast<const std::int32_t*>(code + i + 1) == target) {
				site = here;
				++calls;
			}
			if (std::equal(kNop5.begin(), kNop5.end(), code + i)) {
				++nops;
			}
		}
		if (calls == 1) {
			return site;
		}
		a_why = calls == 0 ? (nops ? "no call to the upkeep (already patched?)"sv : "no call to the upkeep"sv) :
		                     "more than one call to the upkeep"sv;
		return 0;
	}
}

SKSEPluginInfo(
	.Version = Plugin::VERSION,
	.Name = Plugin::NAME,
	.Author = "Yuof"sv,
	.RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary)

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, { .log = true });
	const auto version = REL::Module::get().version();
	logger::info("{} v{} on {}"sv, Plugin::NAME, Plugin::VERSION.string(), version.string());
	// SE and AE code was read; VR was not (no VR executable to check its ids and code against)
	if (REL::Module::IsVR()) {
		logger::error("VR is not supported: inactive"sv);
		return true;
	}
	std::string_view why;
	const auto site = FindCall(why);
	if (!site) {
		logger::error("free-camera player update: {} - not patched"sv, why);
		return true;
	}
	REL::safe_write(site, kNop5.data(), kNop5.size());
	logger::info("patched: the free-camera player update no longer runs the combat-group upkeep on the main thread (+0x{:X})"sv,
		site - kFreeCameraUpdate.address());
	return true;
}
