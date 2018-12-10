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


%description
This tool can be inserted in a pipe between `zfs send` and `rbd import`,
allowing zvol snapshots to be sent to scalable, distributed storage.

%prep
%autosetup


%build
# No build steps yet...

%install
# No install steps yet...

%files
# No files yet other than the license
%license LICENSE



%changelog
* Mon Dec 10 2018 Neal Gompa <ngompa@datto.com>
- Initial packaging skeleton
