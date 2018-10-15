# UAVCAN-experimentation
A small private repo for experiments with UAVCAN

## Getting started

1. Install pyuavcan
    ```bash
    $ python3 -m pip install uavcan
    ```
2. Compile 
    ```bash
    $ cmake . && make
    ```
3. Execute the program
    1. Configure socketcan
        ```bash
        $ sudo ./config_vcan.sh
        ```
    2. Start
        ```bash
        $ ./example vcan0 <nodeID>
        ```

