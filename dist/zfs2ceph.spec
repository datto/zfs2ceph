Name:           zfs2ceph
Version:        0.0.0
Release:        1%{?dist}
Summary:        Convert ZFS sends to the Ceph import format

%if %{_vendor} == "debbuild"
Packager:       Neal Gompa <ngompa@datto.com>
Group:          admin
License:        LGPL-2.1+
%else
Group:          System Environment/Base
License:        LGPLv2+
%endif

URL:            https://github.com/datto/zfs2ceph
Source0:        %{url}/archive/%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make

%description
This tool can be inserted in a pipe between `zfs send` and `rbd import`,
allowing zvol snapshots to be sent to scalable, distributed storage.

%prep
%autosetup


%build
%make_build CCFLAGS="%{optflags}"

%install
mkdir -p %{buildroot}%{_bindir}
install -pm 0755 %{name} %{buildroot}%{_bindir}/%{name}

%files
%doc README.md
%license LICENSE
%{_bindir}/%{name}


%changelog
* Wed Dec 12 2018 Neal Gompa <ngompa@datto.com>
- Set up package build

* Mon Dec 10 2018 Neal Gompa <ngompa@datto.com>
- Initial packaging skeleton
