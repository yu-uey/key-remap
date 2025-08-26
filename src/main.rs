use evdev_rs::{Device, DeviceWrapper, ReadFlag};
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

    loop {
        let (_,ev) = d.next_event(ReadFlag::NORMAL | ReadFlag::BLOCKING)?;
        println!("{} {}", ev.value, ev.event_code);
    }
}
