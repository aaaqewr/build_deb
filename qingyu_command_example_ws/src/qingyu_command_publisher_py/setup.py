from setuptools import find_packages, setup

package_name = "qingyu_command_publisher_py"

setup(
    name=package_name,
    version="0.0.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="todo",
    maintainer_email="todo@todo.todo",
    description="Publish qingyu_api/msg/QingyuCommand from Python.",
    license="Apache-2.0",
    entry_points={
        "console_scripts": [
            "qingyu_command_pub = qingyu_command_publisher_py.qingyu_command_pub:main",
        ],
    },
)
