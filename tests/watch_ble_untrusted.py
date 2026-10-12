"""Independent unpaired-central smoke test (pip install bleak in a disposable venv).

Disconnect the trusted Pixel first, keep its bond on the watch, and capture serial
logs. A connect failure alone is INCONCLUSIVE: require the firmware's explicit
Untrusted connection rejected log to prove policy rejection rather than RF failure.
Never pairs, changes a control, or prints private values.
"""
import asyncio
from bleak import BleakClient, BleakScanner

SERVICE = "7e570001-8e7a-4b6d-9f2a-6c4d3e2f1000"
PRIVATE = "7e570003-8e7a-4b6d-9f2a-6c4d3e2f1000"


async def main():
    device = await BleakScanner.find_device_by_filter(
        lambda device, adv: SERVICE in adv.service_uuids, timeout=15
    )
    if device is None:
        raise RuntimeError("INCONCLUSIVE: watch not advertising; disconnect Pixel first")
    client = BleakClient(device, pair=False, timeout=12, winrt={"use_cached_services": False})
    try:
        await asyncio.wait_for(client.connect(), 15)
        try:
            await asyncio.wait_for(client.read_gatt_char(PRIVATE), 5)
        except Exception as exc:
            print(f"Private read denied ({type(exc).__name__}); correlate watch rejection log")
        else:
            raise AssertionError("FAIL: untrusted central read private steps")
    except AssertionError:
        raise
    except Exception as exc:
        print(f"Independent central connection denied ({type(exc).__name__}); correlate watch rejection log")
    finally:
        await client.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
