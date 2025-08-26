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

enum Spc {
    None,
    Spc,
    Ctrl,
}

fn main() -> Result<(), io::Error> {
    println!("Hello, world!");

    let Some(mut d) = find_device("keyboard")? else {
        println!("No matching device found");
        return Ok(());
    };
    let v = UInputDevice::create_from_device(&d)?;

    const SPACE: EventCode = EventCode::EV_KEY(EV_KEY::KEY_SPACE);
    const LCTRL: EventCode = EventCode::EV_KEY(EV_KEY::KEY_LEFTCTRL);

    d.grab(evdev_rs::GrabMode::Grab)?;
    let mut state = Spc::None;
    loop {
        let (_,ev) = d.next_event(ReadFlag::NORMAL | ReadFlag::BLOCKING)?;
        let (ns, pass, extra) = match (&state, ev.value, ev.event_code) {
            // space pressed => hold state
            (Spc::None, 1, SPACE) => (Spc::Spc, false, None),
            // space released as space => press spc + forward
            (Spc::Spc, 0, SPACE) => (Spc::None, true, Some((SPACE, 1))),
            // space released as ctrl => release ctrl
            (Spc::Ctrl, 0, SPACE) => (Spc::None, false, Some((LCTRL, 0))),
            // any key simul with space => press ctrl + forward
            (Spc::Spc, 1, _) => (Spc::Ctrl, true, Some((LCTRL, 1))),
            // space repeat => noop
            (_, 2, SPACE) => (state, false, None),
            // anything else => forward
            _ => (state, true, None),
        };
        state = ns;
        if let Some((key,press)) = extra {
            v.write_event(&InputEvent {
                time: ev.time,
                event_code: key,
                value: press,
            })?;
        }
        if pass {
            v.write_event(&ev)?;
        }
        v.write_event(&InputEvent {
            time: ev.time,
            event_code: EventCode::EV_SYN(EV_SYN::SYN_REPORT),
            value: 0,
        })?;
    };
    // ungrabbed automatically when program is killed
    // d.grab(evdev_rs::GrabMode::Ungrab)?;
}
