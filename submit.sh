#!/bin/sh
#SBATCH --error=job.%J.err 
#SBATCH --output=job.%J.out
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=2
#SBATCH --time=00:05:00
#SBATCH --mem-per-cpu=2048

mpirun -np $1 -H worker1,worker2 ./game_of_life $2
