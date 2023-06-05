# 2GB buffer
echo "256"
sudo ./mlc --idle_latency -c0 -J/mnt/pmem0
sudo ./mlc --idle_latency -c0 -l256 -J/mnt/pmem0

sudo ./mlc --idle_latency -c0 -J/mnt/pmem1
sudo ./mlc --idle_latency -c0 -l256 -J/mnt/pmem1

echo "64"
sudo ./mlc --idle_latency -c0 -J/mnt/pmem0
sudo ./mlc --idle_latency -c0 -l64 -J/mnt/pmem0

sudo ./mlc --idle_latency -c0 -J/mnt/pmem1
sudo ./mlc --idle_latency -c0 -l64 -J/mnt/pmem1

echo "32"
sudo ./mlc --idle_latency -c0 -J/mnt/pmem0
sudo ./mlc --idle_latency -c0 -l32 -J/mnt/pmem0

sudo ./mlc --idle_latency -c0 -J/mnt/pmem1
sudo ./mlc --idle_latency -c0 -l32 -J/mnt/pmem1