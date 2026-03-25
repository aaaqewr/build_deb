#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

ROS_DISTRO="${ROS_DISTRO:-humble}"
OS_VERSION="${OS_VERSION:-jammy}"

if [ -f "/opt/ros/${ROS_DISTRO}/setup.bash" ]; then
  source "/opt/ros/${ROS_DISTRO}/setup.bash"
fi

unset PYTHONHOME || true
export PYTHONNOUSERSITE=1
if [ -n "${PYTHONPATH:-}" ]; then
  PYTHONPATH="$(printf '%s' "$PYTHONPATH" | tr ':' '\n' | grep -v "${HOME}/.local" | paste -sd: - || true)"
  export PYTHONPATH
fi

ensure_qingyu_api_numpy_include() {
  local export_file="/opt/ros/${ROS_DISTRO}/share/qingyu_api/cmake/export_qingyu_api__rosidl_generator_pyExport.cmake"
  if [ ! -f "$export_file" ]; then
    return 0
  fi

  local bad_path=""
  bad_path="$(grep -oP 'INTERFACE_INCLUDE_DIRECTORIES\\s+\"\\K[^\"]+' "$export_file" 2>/dev/null || true)"
  if [ -z "$bad_path" ]; then
    return 0
  fi

  if [ -e "$bad_path" ]; then
    return 0
  fi

  local numpy_include=""
  numpy_include="$(python3 - <<'PY' 2>/dev/null || true
import numpy
print(numpy.get_include())
PY
)"
  if [ -z "$numpy_include" ]; then
    numpy_include="/usr/lib/python3/dist-packages/numpy/core/include"
  fi

  if [ ! -d "$numpy_include" ]; then
    echo "Error: 找不到 numpy include 目录: $numpy_include"
    echo "请安装: sudo apt install python3-numpy python3-dev"
    exit 1
  fi

  local parent_dir
  parent_dir="$(dirname "$bad_path")"

  if mkdir -p "$parent_dir" 2>/dev/null; then
    ln -sfn "$numpy_include" "$bad_path"
    return 0
  fi

  echo "Error: qingyu_api 导出文件引用了不存在的路径，但当前用户无法创建：$bad_path"
  echo "建议修复 /opt/ros/${ROS_DISTRO}/share/qingyu_api/cmake/export_qingyu_api__rosidl_generator_pyExport.cmake"
  echo "将 INTERFACE_INCLUDE_DIRECTORIES 改为本机 numpy include（例如: $numpy_include）"
  exit 1
}

workspace_mode_build_deb() {
  local root_dir="$SCRIPT_DIR"
  local pkg_name="${PKG_NAME:-qinyu-mechanical-arm}"
  local version="${VERSION:-}"
  local arch
  arch="$(dpkg --print-architecture)"

  if [ -z "$version" ] && [ -f "$root_dir/src/kinematics_model/package.xml" ]; then
    version="$(grep -oP '(?<=<version>)[^<]+' "$root_dir/src/kinematics_model/package.xml" | head -n 1 || true)"
  fi
  if [ -z "$version" ]; then
    version="0.0.0"
  fi

  local pkgroot="/tmp/pkgroot_${pkg_name}_$(date +%s)"
  local deb_out="${root_dir}/${pkg_name}_${version}_${arch}.deb"

  local shortcut_vis="${SHORTCUT_VIS:-kinematics_model_visualization}"
  local shortcut_base="${SHORTCUT_BASE:-kinematics_base_visualization}"
  local shortcut_cam="${SHORTCUT_CAM:-kinematics_camera_visualization}"

  ensure_qingyu_api_numpy_include

  cd "$root_dir"
  if [ -n "${PACKAGES_SELECT:-}" ]; then
    colcon build --packages-select ${PACKAGES_SELECT} --cmake-args -DBUILD_TESTING=OFF -DPython3_EXECUTABLE=/usr/bin/python3
  else
    colcon build --cmake-args -DBUILD_TESTING=OFF -DPython3_EXECUTABLE=/usr/bin/python3
  fi

  rm -rf "$pkgroot" "$deb_out"
  mkdir -p "$pkgroot/DEBIAN"
  mkdir -p "$pkgroot/opt/qinyu_mechanical_arm_camera/install"
  mkdir -p "$pkgroot/usr/bin"
  mkdir -p "$pkgroot/etc/profile.d"

  cp -r "$root_dir/install/"* "$pkgroot/opt/qinyu_mechanical_arm_camera/install/"

  cat > "$pkgroot/DEBIAN/control" <<EOF
Package: ${pkg_name}
Version: ${version}
Section: utils
Priority: optional
Architecture: ${arch}
Depends: ros-${ROS_DISTRO}-ros-base, ros-${ROS_DISTRO}-rviz2, ros-${ROS_DISTRO}-robot-state-publisher, ros-${ROS_DISTRO}-joy, libeigen3-dev, python3-numpy
Maintainer: $(git config user.name 2>/dev/null || echo "maintainer") <$(git config user.email 2>/dev/null || echo "maintainer@example.com")>
Description: Mechanical arm ROS2 workspace install prefix.
EOF

  cat > "$pkgroot/usr/bin/$shortcut_vis" <<EOF
#!/usr/bin/env bash
set -e
source /opt/ros/${ROS_DISTRO}/setup.bash
source /opt/qinyu_mechanical_arm_camera/install/setup.bash
exec ros2 launch kinematics_model model_visualization.launch.py
EOF

  cat > "$pkgroot/usr/bin/$shortcut_base" <<EOF
#!/usr/bin/env bash
set -e
source /opt/ros/${ROS_DISTRO}/setup.bash
source /opt/qinyu_mechanical_arm_camera/install/setup.bash
exec ros2 launch kinematics_model base_model_visulization.launch.py
EOF

  cat > "$pkgroot/usr/bin/$shortcut_cam" <<EOF
#!/usr/bin/env bash
set -e
source /opt/ros/${ROS_DISTRO}/setup.bash
source /opt/qinyu_mechanical_arm_camera/install/setup.bash
exec ros2 launch kinematics_model camera_visulization.launch.py
EOF

  chmod +x "$pkgroot/usr/bin/$shortcut_vis" "$pkgroot/usr/bin/$shortcut_base" "$pkgroot/usr/bin/$shortcut_cam"

  cat > "$pkgroot/etc/profile.d/qinyu_mech_arm.sh" <<EOF
if [ -f /opt/ros/${ROS_DISTRO}/setup.bash ]; then
  . /opt/ros/${ROS_DISTRO}/setup.bash
fi
if [ -f /opt/qinyu_mechanical_arm_camera/install/setup.bash ]; then
  . /opt/qinyu_mechanical_arm_camera/install/setup.bash
fi
EOF

  dpkg-deb --build "$pkgroot" "$deb_out"

  local deb_public="/tmp/$(basename "$deb_out")"
  cp -f "$deb_out" "$deb_public"
  chmod 0644 "$deb_public"

  echo ""
  echo "=========================================="
  echo "构建完成！"
  ls -lh "$deb_out"
  echo "安装命令: sudo apt install $deb_public"
  echo "=========================================="
}

single_package_mode_build_deb() {
  local pkg_name
  local pkg_version
  pkg_name="$(grep -oP '(?<=<name>)[^<]+' package.xml | head -n 1)"
  pkg_version="$(grep -oP '(?<=<version>)[^<]+' package.xml | head -n 1)"

  local custom_name="${CUSTOM_NAME:-qingyu-robot-interface}"

  echo "=========================================="
  echo "Package: $pkg_name"
  echo "Version: $pkg_version"
  echo "Custom name: $custom_name"
  echo "=========================================="

  export ROSDISTRO_INDEX_URL="file://${SCRIPT_DIR}/rosdistro/index-v4.yaml"
  export ROSDISTRO_INDEX_FILE="${SCRIPT_DIR}/rosdistro/index-v4.yaml"

  if [ ! -d "rosdistro" ]; then
    mkdir -p rosdistro
    cat > rosdistro/index-v4.yaml << 'EOF'
---
type: index
version: 4
distributions:
  humble:
    distribution: [humble/distribution.yaml]
    distribution_cache: http://localhost/invalid
    distribution_status: active
    distribution_type: ros2
    python_version: 3
EOF

    mkdir -p rosdistro/humble
    cat > rosdistro/humble/distribution.yaml << EOF
---
release_platforms:
  ubuntu:
  - jammy
repositories: {}
type: distribution
version: 2
EOF
  fi

  rm -rf debian/ obj-* ../*.deb ./*.deb

  export BLOOM_SKIP_ROSDISTRO_CHECK=1
  export ROSDEP_SKIP_KEYS="rosidl_default_generators rosidl_default_runtime builtin_interfaces"

  if [ ! -d ".git" ]; then
    git init --quiet
    git add . --all 2>/dev/null || true
    git commit -m "init" --quiet 2>/dev/null || true
  fi

  mkdir -p debian
  cat > debian/control << EOF
Source: ${custom_name}
Section: misc
Priority: optional
Maintainer: $(git config user.email 2>/dev/null || echo "maintainer@example.com")
Build-Depends: debhelper (>= 9.0.0), cmake, python3-dev, python3-numpy, ros-${ROS_DISTRO}-ros-workspace, ros-${ROS_DISTRO}-rosidl-default-generators, ros-${ROS_DISTRO}-builtin-interfaces
Homepage: https://example.com

Package: ${custom_name}
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}, python3-numpy, ros-${ROS_DISTRO}-ros-workspace, ros-${ROS_DISTRO}-rosidl-default-runtime, ros-${ROS_DISTRO}-builtin-interfaces
Description: ${pkg_name} ROS2 package
EOF

  local date_rfc
  date_rfc="$(date -R)"
  cat > debian/changelog << EOF
${custom_name} (${pkg_version}-0${OS_VERSION}) ${OS_VERSION}; urgency=high

  * Initial release

 -- $(git config user.name 2>/dev/null || echo "Maintainer") <$(git config user.email 2>/dev/null || echo "maintainer@example.com")>  ${date_rfc}
EOF

  cat > debian/rules << 'EOF'
#!/usr/bin/make -f

export DH_VERBOSE = 1
export PYTHON3_INTERPRETER = /usr/bin/python3
export ROS_DISTRO = humble

%:
	dh $@ --buildsystem=cmake --parallel

override_dh_auto_configure:
	dh_auto_configure -- \
		-DCMAKE_INSTALL_PREFIX=/opt/ros/$(ROS_DISTRO) \
		-DAMENT_PREFIX_PATH=/opt/ros/$(ROS_DISTRO) \
		-DCMAKE_PREFIX_PATH=/opt/ros/$(ROS_DISTRO) \
		-DBUILD_TESTING=OFF

override_dh_auto_test:
	true

override_dh_auto_install:
	dh_auto_install
	if [ -d debian/tmp/opt/ros/$(ROS_DISTRO) ]; then \
		mkdir -p debian/$(DEB_SOURCE)/opt/ros/$(ROS_DISTRO); \
		cp -r debian/tmp/opt/ros/$(ROS_DISTRO)/* debian/$(DEB_SOURCE)/opt/ros/$(ROS_DISTRO)/ || true; \
	fi

override_dh_shlibdeps:
	dh_shlibdeps --dpkg-shlibdeps-params=--ignore-missing-info 2>/dev/null || true

override_dh_fixperms:
	true

override_dh_builddeb:
	dh_builddeb -- --root-owner-group

override_dh_strip:
	true

override_dh_makeshlibs:
	true
EOF

  chmod +x debian/rules
  echo "12" > debian/compat
  mkdir -p debian/source
  echo "3.0 (quilt)" > debian/source/format

  echo "开始构建..."
  local arch
  arch="$(dpkg --print-architecture)"
  local deb_out="${SCRIPT_DIR}/${custom_name}_${pkg_version}-0${OS_VERSION}_${arch}.deb"
  local build_dir="${SCRIPT_DIR}/obj-deb-${arch}"
  local stage_dir="${SCRIPT_DIR}/deb-stage"

  rm -rf "$build_dir" "$stage_dir" "$deb_out"
  mkdir -p "$build_dir" "$stage_dir"

  cmake -S "$SCRIPT_DIR" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="/opt/ros/${ROS_DISTRO}" \
    -DAMENT_PREFIX_PATH="/opt/ros/${ROS_DISTRO}" \
    -DCMAKE_PREFIX_PATH="/opt/ros/${ROS_DISTRO}" \
    -DPython3_EXECUTABLE=/usr/bin/python3 \
    -DBUILD_TESTING=OFF

  cmake --build "$build_dir" --parallel
  DESTDIR="$stage_dir" cmake --install "$build_dir"

  mkdir -p "$stage_dir/DEBIAN"
  local installed_size
  installed_size="$(du -ks "$stage_dir" | awk '{print $1}')"

  local maint_name
  local maint_email
  maint_name="$(git config user.name 2>/dev/null || echo "Maintainer")"
  maint_email="$(git config user.email 2>/dev/null || echo "maintainer@example.com")"

  cat > "$stage_dir/DEBIAN/control" << EOF
Package: ${custom_name}
Version: ${pkg_version}-0${OS_VERSION}
Section: misc
Priority: optional
Architecture: ${arch}
Maintainer: ${maint_name} <${maint_email}>
Installed-Size: ${installed_size}
Depends: python3-numpy, ros-${ROS_DISTRO}-ros-workspace, ros-${ROS_DISTRO}-rosidl-default-runtime, ros-${ROS_DISTRO}-builtin-interfaces
Description: ${pkg_name} ROS2 package
EOF

  mkdir -p "$stage_dir/etc/profile.d"
  cat > "$stage_dir/etc/profile.d/qingyu_robot_interface.sh" << EOF
if [ -f /opt/ros/${ROS_DISTRO}/setup.bash ]; then
  . /opt/ros/${ROS_DISTRO}/setup.bash
fi
EOF

  cat > "$stage_dir/DEBIAN/postinst" << 'EOF'
#!/bin/sh
set -e

MARK_BEGIN="# >>> qingyu-robot-interface >>>"
MARK_END="# <<< qingyu-robot-interface <<<"
LINE_1='if [ -f /etc/profile.d/qingyu_robot_interface.sh ]; then'
LINE_2='  . /etc/profile.d/qingyu_robot_interface.sh'
LINE_3='fi'

if [ -f /etc/bash.bashrc ]; then
  if ! grep -Fq "$MARK_BEGIN" /etc/bash.bashrc; then
    {
      echo ""
      echo "$MARK_BEGIN"
      echo "$LINE_1"
      echo "$LINE_2"
      echo "$LINE_3"
      echo "$MARK_END"
    } >> /etc/bash.bashrc
  fi
fi

exit 0
EOF

  cat > "$stage_dir/DEBIAN/postrm" << 'EOF'
#!/bin/sh
set -e

MARK_BEGIN="# >>> qingyu-robot-interface >>>"
MARK_END="# <<< qingyu-robot-interface <<<"

if [ "$1" = "purge" ] && [ -f /etc/bash.bashrc ]; then
  tmp="$(mktemp)"
  awk -v b="$MARK_BEGIN" -v e="$MARK_END" '
    $0 == b {skip=1; next}
    $0 == e {skip=0; next}
    !skip {print}
  ' /etc/bash.bashrc > "$tmp"
  cat "$tmp" > /etc/bash.bashrc
  rm -f "$tmp"
fi

exit 0
EOF

  chmod 0755 "$stage_dir/DEBIAN/postinst" "$stage_dir/DEBIAN/postrm"

  dpkg-deb --build --root-owner-group "$stage_dir" "$deb_out"

  local deb_public="/tmp/$(basename "$deb_out")"
  cp -f "$deb_out" "$deb_public"
  chmod 0644 "$deb_public"

  echo ""
  echo "=========================================="
  echo "构建完成！"
  ls -lh "$deb_out"
  echo "安装命令: sudo apt install $deb_public"
  echo "=========================================="
}

if [ -f "$SCRIPT_DIR/package.xml" ]; then
  single_package_mode_build_deb
else
  workspace_mode_build_deb
fi
