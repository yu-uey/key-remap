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
enum Action {
    Press,
    Release,
    Tap,
    None,
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
    const LCTRL: EventCode = EventCode::EV_KEY(EV_KEY::KEY_LEFTCTRL);
    const ESC: EventCode = EventCode::EV_KEY(EV_KEY::KEY_ESC);

    const TAPTERM: i64 = 100 * 1000;
    
    const KEY: EventCode = CAPS;
    const TAP: EventCode = ESC;
    const HOLD: EventCode = LCTRL;
    
    let mut s = State::R;
    d.grab(evdev_rs::GrabMode::Grab)?;
    loop {
        let (_,ev) = d.next_event(ReadFlag::NORMAL | ReadFlag::BLOCKING)?;
        let (ns, pass, extra) = match (&s, ev.event_code, ev.value) {
            // start motion
            (State::R, KEY, 1) => (State::P(ev.time), false, Action::None),
            // other key pressed during hold
            // => start hold + pass key
            (State::P(t), _, 1) if ev.time.past(t, TAPTERM) => (State::H, true, Action::Press),
            // other key pressed during tap
            // => send tap + pass key
            (State::P(_), _, 1) => (State::R, true, Action::Tap),
            // release after nothing
            // => send tap
            (State::P(_), KEY, 0) => (State::R, false, Action::Tap),
            // release after hold
            // => release hold
            (State::H, KEY, 0) => (State::R, false, Action::Release),
            _ => (s, true, Action::None),
        };
        s = ns;
        match extra {
            Action::Press => {
                v.write_event(&InputEvent {
                    event_code: HOLD,
                    value: 1,
                    time: ev.time,
                })?;
            },
            Action::Release => {
                v.write_event(&InputEvent {
                    event_code: HOLD,
                    value: 0,
                    time: ev.time,
                })?;
            },
            Action::Tap => {
                v.write_event(&InputEvent {
                    event_code: TAP,
                    value: 1,
                    time: ev.time,
                })?;
                v.write_event(&InputEvent {
                    event_code: TAP,
                    value: 0,
                    time: ev.time,
                })?;
            },
            _ => (),
        };
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
