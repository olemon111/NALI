g++ nvm_test.cpp -lpthread -lpmem -o nvm_test
sudo wrmsr --all 0x1a4 15
for io_size in 256 #32 64 256 
do
    sudo ./nvm_test 1 1 0 0 $io_size
    sudo ./nvm_test 1 1 1 0 $io_size
    sudo ./nvm_test 1 0 0 0 $io_size
    sudo ./nvm_test 1 0 1 0 $io_size
done
sudo wrmsr --all 0x1a4 0 b