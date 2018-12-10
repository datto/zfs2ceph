# zfs2ceph

This program enables converting ZFS sends to the Ceph import format.

In order to do the conversion, this can be inserted in a pipe between
`zfs send` and `rbd import`, allowing zvol snapshots to be sent to Ceph,
the scalable, distributed storage.

## License

zfs2ceph is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
