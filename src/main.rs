use evdev_rs::{enums::{EventCode, EV_KEY, EV_SYN}, Device, DeviceWrapper, InputEvent, ReadFlag, UInputDevice};
use std::{fs, io};

fn find_device(search: &str) -> io::Result<Option<Device>> {
    for i in 0.. {
        let path = format!("/dev/input/event{}", i);
        let Ok(true) = fs::exists(&path) else {
            println!("No more inputs");
            break;
        };
        let d = fs::File::open(path)
                .and_then(|f| Device::new_from_file(f))?;
        if d.name().is_some_and(|n| n.contains(search)) {
            return Ok(Some(d));
        }
    }
    Ok(None)
}

fn main() -> Result<(), io::Error> {
    println!("Hello, world!");

    let Some(d) = find_device("keyboard")? else {
        println!("No matching device found");
        return Ok(());
    };
    let v = UInputDevice::create_from_device(&d)?;

    const SPACE: EventCode = EventCode::EV_KEY(EV_KEY::KEY_SPACE);
    const LCTRL: EventCode = EventCode::EV_KEY(EV_KEY::KEY_LEFTCTRL);

    loop {
        let (_,ev) = d.next_event(ReadFlag::NORMAL | ReadFlag::BLOCKING)?;
        let (pass, extra) = match (ev.value, ev.event_code) {
            // map space -> lctrl
            (v, SPACE) => (false, Some(InputEvent {
                time: ev.time,
                event_code: LCTRL,
                value: v,
            })),
            _ => (true, None),
        };
        if let Some(extra_ev) = extra {
            v.write_event(&extra_ev)?;
        }
        if pass {
            v.write_event(&ev)?;
        }
        v.write_event(&InputEvent {
            time: ev.time,
            event_code: EventCode::EV_SYN(EV_SYN::SYN_REPORT),
            value: 0,
        })?;
    }
}
