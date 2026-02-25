# :unlock: CambucaHEX
CambucaHEX is an alternative implementation and experimental extension
based on the original work [kpurens](https://github.com/kpurens)
Please read the original license file for attribution and usage terms.

Tool for reconstructing partially known HEX RAW private keys.. <br/>
Performs Secp256k1 Point Multiplication directly on GPU. <br/>

## :heavy_check_mark: When to use CambucaHEX
CambucaHEX is designed for recovering private keys when missing hexadecimal characters are scattered across the key rather than forming a continuous range.
It is particularly useful when keys were partially corrupted,manually copied, or truncated at random positions.

## Modes
-x / --hexx
Scattered HEX mode. Use when unknown characters are distributed across the key. Unknown characters must be marked with x.
<img width="1624" height="300" alt="exemple" src="https://github.com/user-attachments/assets/6c5dfba7-3e38-4893-a56e-55e3fb1b7a92" />

### -l / --linear
Linear mode uses a sliding window approach over the private key.
Instead of testing all unknown positions simultaneously, a fixed‑size window moves across the key and brute‑forces consecutive HEX characters within that window.
The key will only be recovered if the corrupted or missing characters are located inside the active window region.
This mode is primarily provided as a conceptual / experimental approach.
Because the search is sequential, specialized tools that use incremental point‑addition techniques can achieve significantly higher performance for this type of search. 

### -c / --combination
Combination mode tests all combinations of unknown HEX positions
across the entire key space.
The value of `-p` defines how many corrupted characters may exist,
independent of their position. For example, `-p 2` searches for
any two unknown HEX characters anywhere in the key.

| Option              | Description                                                                                      |
| ------------------- | ------------------------------------------------------------------------------------------------ |
| `-x, --hexx`        | Scattered HEX mode. Use when unknown characters are distributed; mark unknowns with `x`.         |
| `-l, --linear`      | Sliding window scan over consecutive HEX positions (experimental concept mode).                  |
| `-c, --combination` | Tests all combinations of unknown positions; use `-p` to set how many characters may be unknown. |
| `-p N`              | Number of unknown HEX characters (used with `-c` and `-l`).                                      |
| `-k HEX`            | Partial key. If not provided, `chave.txt` will be used.                                          |
| `-h, --help`        | Show the help message.                                                                           |


#### linear
./Cambuca -l -p 4 -k e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

#### Combination mode
./Cambuca -c -p 2 -k e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

#### Scattered HEX mode
./Cambuca -x -k e3b0c44298fc1c149afbf4c8996fb92427ae41ex649b934ca49x991bx85xbx5x

## :x: When NOT to use CambucaHEX
- If the missing characters are **sequential or form a continuous range**, other specialized tools may be more efficient. CambucaHEX should be avoided **when it's possible to derive private keys from each-other.** <br> In such cases CambucaHEX is sub-optimal as it would be much quicker to re-use already calculated public keys.<br>
- Bitcoin Puzzle (Where you have to simply increment the private key very quickly)<br>
- WIF Recovery,Wait for CambucaWIF hahhahah<br>

## :notebook_with_decorative_cover: Design
System design can be illustrated with data-flow diagram:
![diagrama](https://github.com/user-attachments/assets/4e3d7bb7-854b-4a86-813e-883f7b17014c)

## :spiral_notepad: Requirements
- Linux Operating System
- Nvidia GPU
- Nvidia Display Driver
- Nvidia Cuda Toolkit

## :wrench: Quick Setup
1. Clone or download the repository
2. Find Compute Capability of your Nvidia GPU in [developer.nvidia.com/cuda-gpus](https://developer.nvidia.com/cuda-gpus)
3. Edit Makefile **CCAP** value to your Compute Capability (as an Integer, without decimal places!)
4. Edit Makefile **CUDA** value to your Nvidia Cuda Toolkit directory
5. Open terminal and run `make all` to generate the binaries (don't need administrator privileges)
6. Run `./Cambuca` to launch the test cases (execution should take about ~3 seconds)
If you see an error message `system has unsupported display driver / cuda driver combination` or `forward compatibility was attempted on non supporte HW` that means your cuda toolkit is incompatible with your display driver. (can try installing another display driver or another cuda toolkit version + restarting your PC).

## :gear: Configuration
You will need to set-up your IDE (development environment) to link the Cuda Toolkit / dependencies.<br />
I personally use VS Code, it has the `c_cpp_properties.json` configuration file.<br />
In this file it's important to set correct `includePath` and `compilerPath`, otherwise you'll see compiler errors.<br />
<br />
All other configuration is done through `GPU/GPUSecp.h` file.<br />
It contains comments / info about each configurable value, so it should be relatively simple.<br />
You will need to adjust `BLOCKS_PER_GRID` and `THREADS_PER_BLOCK` based on your GPU architecture.<br />
After editing this file you will need to rebuild the project with `make clean` and `make all`<br />

## :rocket: Real Launch
Once you are satisfied with the test cases / performance, you can setup the data for real-world launch:

1. Select Recovery Addresses:<br />
  1.1 You can select one or more specific addresses that you wish to recover<br />
  1.2 Optionally you can obtain non-zero addresses for currencies of your choice<br />
  1.3 [Blockchair](https://blockchair.com/dumps) is one of the websites that provide such data<br />
2. Use the `addr_to_hash.py` tool to convert chosen addresses into Hash160 files
3. Create folder for holding real Hash160 files (you shouldn't mix them with the test hashes)
4. Move your newly generated Hash160 files into the real Hash160 folder (they will be combined upon launch)
5. Edit `GPU/GPUSecp.h` configuration values that have changed<br />
6. Execute `make clean`, `make all` and `./Cambuca` to launch the application.

## :memo: Implementation
### Private Key Generation
The only input for ECDSA Public Key calculation is 32-byte number. (also called scalar or private key)<br>
While it should always be randomly generated, it can also be chosen manually in many different ways.<br>
This project focuses on recovering private keys that were chosen from seed phrase and then partially lost / forgotten.<br>
Current implementation combines two words and then performs SHA256 Tranformation to obtain exactly 32 bytes.<br>
You could also use different hashing algorithm (SHA1 / SHA512 / Keccak / Ripemd / etc) or repeat it multiple times.<br>
One side-note about the current implementation of SHA256 function:<br>
It is quite complex, as it takes integers as input and outputs integers as output.<br>
Thus every four bytes will have inverse-order and it can be very confusing to work with.<br>
But it's extremely fast and much faster than than the [Chuda-Hashing-Algos implementation](https://github.com/mochimodev/cuda-hashing-algos/blob/master/sha256.cu)<br>


### Secp256k1 Point Multiplication
The main problem for doing Point Multiplication on GPUs is that it's simply too many calculations.<br>
To multiply Secp256k1 point G with scalar you have to perform ~255 Point-Doubling operations and ~127 Point-Addition operations.<br>
Each Point-Doubling and Point-Addition operation has several expensive `mul_mod` / `add_mod` / `sub_mod` operations.<br>
This is computationally very intensive for one kernel thread to handle. (especially with limiter register / memory size)<br>
However all of the values in Secp256k1 calculations (except for the scalar vector) are constant and never change.<br>
This allows pre-computing some values and passing them to kernel directly. (either through global memory or constants)<br>
In this implementation i chose to pre-compute 16 chunks, 2-bytes each, for a total of 1048576 Points. (taking ~67MB of space)<br>
To calculate the public key we simply need to add these 16 chunks together and perform modular inverse.<br>
Which is around ~20 times more efficient than doing Point Multiplication without pre-computation.<br>
However this comes with a performance penalty of having to frequently access global memory from GPU threads.<br>

### Hash160 / Address
Simple hashing operation can be performed to obtain the actual address from public key.<br>
Bitcoin Wiki has an excellent article & diagram about this: [Bitcoin_addresses](https://en.bitcoin.it/wiki/Technical_background_of_version_1_Bitcoin_addresses)<br>
Basically Hash160 (20-byte hash) is the actual address. (just without the Base58 encoding)<br>
Once the compressed / uncompressed Hash160 is calculated, it is searched in the `inputHashBuffer`.<br>
`inputHashBuffer` is generated by merging / sorting all hash files located in `NAME_HASH_FOLDER` folder.<br>

### Binary Search
In this implementation i use Binary Search to find `uint64_t` (the last 8 bytes) of each hash.<br>
Binary Search was used instead of Bloom Filter for multiple reasons:<br>
- Simplicity (code is smaller and easier to understand / debug)<br>
- Can be verified (If a hash is found - it's guaranteed to be in the inputHashBuffer)<br>
- Should be faster (Bloom Filter requires calculating Murmurhashes and does more calls to globabl memory)<br>

There can still be false positives, since it's only 8 bytes, but the amount of false positives is less than Bloom Filter.<br>


### Alternatives
At first glance the performance numbers may seem lower than other public Secp256k1 Cuda projects.<br>
However an important difference is that we perform **the full point multiplication on each thread**<br>
Unlike most other projects which derive / increment private key and perform just one addition + inverse.<br>
As mentioned in first chapter - this is intentional, allowing us to solve tasks that no other cuda library can solve.<br>
The only other public library that does full Secp256k1 point multiplication on GPUs is [Hashcat](https://github.com/hashcat/hashcat/blob/80229e34e4f09a1decd4ba1cb73e5f067bdc977c/OpenCL/inc_ecc_secp256k1.cl)<br>
After doing some quick testing it seems that Hashcat's implementation is about 20-30 times slower.<br>
This could be attributed to the lack of pre-computed GTable and not using assambler directives.<br>

## :arrow_double_down: Bottlenecks
### Global Memory Access
This is probably the main bottleneck in the entire application.<br>
Global memory (DRAM) access is required by design - there is no way around it.<br>
CudaSecp needs to access Wordlist / GTable / Hash160 buffers in DRAM from GPU.<br>
Wordlist and Hash160 buffers are very efficient, but the main problem is accessing GTable.<br>
GTable is very large and currently doesn't have coalescing. (since each thread accesses random parts of the table)<br>
And we need to access 16*64 bytes of GTable memory for each seed / public-key, which is a lot.<br>

### Register Pressure
Register Pressure is the other main issue with the current implementation.<br>
There are simply too many variables that take too much space in device registers.<br>
Each part of the `CudaRunSecp256k1` function has some important byte arrays / values.<br>
Not to mention that each Secp256k1 point takes 64 bytes of memory to store.<br>
This severely impacts the occupancy and how many threads are used simultaneously.<br>
Basically the threads have to wait until some device registers are finally available.<br>

## :heavy_plus_sign: Optimizations
```diff

+ GTable Chunk Size
It's possible to pre-compute different size chunks for the GTable.
Ideally we would have 8 chunks that are 4-bytes each, which would allow doing only 7 point-additions instead of 15.
However that would require 2TB of DRAM space. (maybe if you're reading this in the year 2030 then it's actually possible.)
It's also possibe to go the other way - by doing 32 chunks that are 1-byte each.
That would take up only 512KB of space, but it's still not small enough to be put into device constants.
I also considered doing some hybrid approach (with 22 or 23-bit chunks), but sadly my hardware doesn't have enough DRAM.

+ Register Management
As mentioned in the Bottlenecks section - Register Pressure is one of the main problems.
One possible solution would be to restructure the CudaRunSecp256k1 function to allow better optimization of variables.
Currently the compiler is forced to remember privKey / ctxSha256 for the entire thread lifetime.
Maybe these values could be put into separate functions to allow the compiler to 'forget' them sooner.

+ Memcpy
Currently the _PointMultiSecp256k1 function uses memcpy to copy bytes from global memory.
Initially points were loaded from memory as 4 Long variables (4 * 8 = 32 bytes for x and y coordiates)
However that resulted in 10% slower performance compared to simply doing memcpy on all 32 bytes.
It's possible that there is (or will be) some CUDA-specific alternative to memcpy - that could be worth researching.

+ GPUSha256 Function
Initially the Sha256 function was taken from CUDA Hashing Algorithms Collection.
But for some reason their implementation stores 64 integers in registers to calculate the hash.
That caused significant reduction in performance, since registers are already a bottleneck.
The current Sha256 is very optimal and causes almost no performance loss.
If you plan on using other Hashing function then you may need to do some benchmarking or optimizations.

+ Cuda Generations
Each new generation of CUDA GPUs adds new important optimizations that improve performance.
As noted in performance section - newer generation RTX3060 performs faster than older generation RTX2070.
The main improvements come from larger GPU registers - as threads won't have to wait as much to store their variables.
So using the latest generation hardware & CUDA drivers has a big advantage.
```

## :gift: Support / Donations

### This Project (Modifications & Maintenance)
If you find this project useful, you can support its maintenance and
continued experimentation:
Bitcoin (BTC): bc1qs850jrz5ktl5vpma0sz40z29392wrzx9cevze


This project is based on the original work by kpurens.
Please consider supporting the original author as well:
[Buy a coffee ☕](https://buymeacoffee.com/kpurens)



## :copyright: Credits / References
- [Jean Luc PONS Secp256k1 Math library](https://github.com/JeanLucPons/VanitySearch) (Nice set of Secp256k1 Math operations / functions)
- [CUDA Hashing Algorithms Collection](https://github.com/mochimodev/cuda-hashing-algos) (Contains multiple useful CUDA hashing algorithms)
- [Secp256k1 Calculator](https://github.com/MrMaxweII/Secp256k1-Calculator) (Excellent tool for verifying / testing Secp256k1 calculations)
- [PrivKeys Database](https://privatekeys.pw/) (Tool used to obtain test seeds / addresses) (**DO NOT** enter real private keys here)
- addr_to_hash Python tool designed by Pieter Wuille (Used to convert non-zero addresses to hash files)
- [kpurens](https://github.com/kpurens) Base project and original implementation that inspired this work.

## :grey_question: Frequently Asked Questions


```Why create new project if Cuda Secp256k1 Math Library already exists?``` <br />
Because Secp256k1 Math Library doesn't actually do point multiplication on GPU. <br />
VanitySearch and similar repos all perform point multiplication on CPU and derive private keys from each-other. <br />

```Which crypto-currencies are currently supported in this repository?```<br />
All cryptos that use Secp256k1 public keys. This includes BTC / BCH / ETH / LTC / Doge / Dash and many others.<br />
For Ethereum the final hashing part is slightly different and will need some adjustments in code.<br />

```Can i only do GPU Point Multiplication? I don't need to generate seeds / find hashes.```<br />
Yes, you can just call **_PointMultiSecp256k1** function from your own custom kernel (and pass GTable from global memory)<br />
Thus this repository can also be used as general-purpose Secp256k1 point multiplication system on GPUs.<br />

```Why is CUDA used instead of OpenCL?```<br />
The GPU Math operations designed by Jean Luc PONS are written for CUDA GPUs.<br />
It would be very hard to re-write them in OpenCL and have similar performance level.<br />
