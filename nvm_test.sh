sudo ./build.sh

# Usage: ./nvm_test thread_nr is_write is_remote

# threads=(1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16)
threads=(16)

echo "========= write local ========="
for t in ${threads[@]}
do
sudo ./build/nvm_test ${t} 1 0 0
sleep 10
done

# echo "========= write remote ========="
# for t in ${threads[@]}
# do
# sudo ./build/nvm_test ${t} 1 1 0
# sleep 10
# done

# echo "========= write remote 2nd ========="
# for t in ${threads[@]}
# do
# sudo ./build/nvm_test ${t} 1 1 1
# sleep 10
# done

# echo "========= read local ========="
# for t in ${threads[@]}
# do
# sudo ./build/nvm_test ${t} 0 0 0
# sleep 10
# done

# echo "========= read remote ========="
# for t in ${threads[@]}
# do
# sudo ./build/nvm_test ${t} 0 1 0
# sleep 10
# done

# echo "========= read remote 2nd ========="
# for t in ${threads[@]}
# do
# sudo ./build/nvm_test ${t} 0 1 1
# sleep 10
# done