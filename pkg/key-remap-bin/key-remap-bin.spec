%global debug_package %{nil}

Name:           key-remap-bin
Version:        0.1.0
Release:        %autorelease
Summary:        Remaps keys for convenient LCtrl

License:        MIT

Source:         ./key-remap-bin-0.1.0.tar.gz

BuildRequires:  libevdev-devel

%description
Remaps keys for convenient LCtrl

%prep
%autosetup

%install
# mkdir -p %{buildroot}/usr/bin
install -DTm755 ./key-remap %{buildroot}/usr/bin/key-remap
# mkdir -p %{buildroot}/usr/lib/systemd/system
install -DTm755 ./key-remap.service %{buildroot}/usr/lib/systemd/system/key-remap.service

%files
/usr/bin/key-remap
/usr/lib/systemd/system/

%changelog
%autochangelog
