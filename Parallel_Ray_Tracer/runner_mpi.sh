#!/bin/bash
#

# This is an example bash script that is used to submit a job
# to the cluster.
#
# Typcially, the # represents a comment. However, #SBATCH is
# interpreted by SLURM to give an option from above. As you 
# will see in the following lines, it is very useful to provide
# that information here, rather than the command line.

# Name of the job - You MUST use a unique name for the job
#SBATCH -J s_vertC

# Standard out and Standard Error output files
#SBATCH -o std/rt_mpi_%j.out
#SBATCH -e std/rt_mpi_%j.err

# In order for this to send emails, you will need to remove the
# space between # and SBATCH for the following 2 commands.
# Specify the recipient of the email
# SBATCH --mail-user=adc2108@rit.edu

# Notify on state change: BEGIN, END, FAIL or ALL
# SBATCH --mail-type=ALL

# spack configuration help
#SBATCH --partition=kgcoe-mps
#SBATCH --account=kgcoe-mps
#SBATCH --get-user-env
# number of tasks 
#SBATCH --ntasks=16
# maximum run duration (your peers will thank you)
#SBATCH --time=0-1:0:0
# maximum memory per node
# SBATCH --mem=15000M
#SBATCH --mem-per-cpu=4000M

#
# Your job script goes below this line.
#

# This preps the system with all the needed libraries and resources
spack env activate cmpe-655

# Place your srun command here
# Notice that you have to provide the number of processes that
# are needed. This number needs to match the number of cores
# indicated by the -n option. If these do not, your results will
# not be valid or you may have wasted resources that others could
# have used.

# The following commands can be used as a starting point for 
# timing. Each example below needs to be modified to fit your parameters
# accordingly. You will also need to modify the config to point to
# box.xml to render the complex scene.
# **********************************************************************
# MAKE SURE THAT YOU ONLY HAVE ONE OF THESE UNCOMMENTED AT A TIME!
# **********************************************************************
# Sequential
# srun -n $SLURM_NPROCS raytrace_mpi -h 100 -w 100 -c configs/twhitted.xml -p none 
# Static Strips Horizontal
# srun -n $SLURM_NPROCS raytrace_mpi -h 100 -w 100 -c configs/twhitted.xml -p static_strips_horizontal 
# Static Strips Vertical
# srun -n $SLURM_NPROCS raytrace_mpi -h 5000 -w 5000 -c configs/twhitted.xml -p static_strips_vertical
# srun -n $SLURM_NPROCS raytrace_mpi -h 5000 -w 5000 -c configs/box.xml -p static_strips_vertical
# Static Cycles
# srun -n $SLURM_NPROCS raytrace_mpi -h 100 -w 100 -c configs/twhitted.xml -p static_cycles_vertical -cs 1
# Static Blocks
# srun -n $SLURM_NPROCS raytrace_mpi -h 100 -w 100 -c configs/twhitted.xml -p static_blocks 
# Dynamic
# srun -n $SLURM_NPROCS raytrace_mpi -h 5000 -w 5000 -c configs/twhitted.xml -p dynamic -bh 7 -bw 13
srun -n $SLURM_NPROCS raytrace_mpi -h 5000 -w 5000 -c configs/box.xml -p dynamic -bh 70 -bw 1
