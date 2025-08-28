use evdev_rs::{enums::{EventCode, EV_KEY, EV_SYN}, Device, DeviceWrapper, InputEvent, ReadFlag, TimeVal, UInputDevice};
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

enum State {
    R, // release
    P(TimeVal), // press
    H, // hold
}

trait TapTerm {
    fn past(&self, prev:&TimeVal, us:i64) -> bool;
}
impl TapTerm for TimeVal {
    fn past(&self, prev:&TimeVal, us:i64) -> bool {
        (if self.tv_sec > prev.tv_sec {self.tv_usec + 1000000} else {self.tv_usec}) > prev.tv_usec + us
    }
}

fn main() -> Result<(), io::Error> {
    println!("Hello, world!");

    let Some(mut d) = find_device("keyboard")? else {
        println!("No matching device found");
        return Ok(());
    };
    let v = UInputDevice::create_from_device(&d)?;

    const CAPS: EventCode = EventCode::EV_KEY(EV_KEY::KEY_CAPSLOCK);
    const SPACE: EventCode = EventCode::EV_KEY(EV_KEY::KEY_SPACE);
    const LCTRL: EventCode = EventCode::EV_KEY(EV_KEY::KEY_LEFTCTRL);

    const TAPTERM: i64 = 100 * 1000;
    
    let mut s = State::R;
    d.grab(evdev_rs::GrabMode::Grab)?;
    loop {
        let (_,ev) = d.next_event(ReadFlag::NORMAL | ReadFlag::BLOCKING)?;
        let (ns, pass, pre) = match (&s, ev.event_code, ev.value) {
            // capslock => ctrl
            (_, CAPS, p) => (s, false, Some((LCTRL, p))),
            // ctrl => capslock
            (_, LCTRL, p) => (s, false, Some((CAPS, p))),

            // start motion
            (State::R, SPACE, 1) => (State::P(ev.time), false, None),
            // other key pressed during hold
            (State::P(t), _, 1) if ev.time.past(t, TAPTERM) => (State::H, true, Some((LCTRL, 1))),
            // other key pressed during tap
            (State::P(_), _, 1) => (State::R, true, Some((SPACE, 1))),
            // release after nothing
            (State::P(_), SPACE, 0) => (State::R, true, Some((SPACE, 1))),
            // release after hold
            (State::H, SPACE, 0) => (State::R, false, Some((LCTRL, 0))),
            _ => (s, true, None),
        };
        s = ns;
        if let Some((key,press)) = pre {
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
