project(autodiff_test002)
set(SOURCE main.cpp)

add_executable(autodiff_test002 ${SOURCE})
target_link_libraries(autodiff_test002 inmost)

if(USE_MPI)
  message("linking autodiff_test002 with MPI")
  target_link_libraries(autodiff_test002 ${MPI_LIBRARIES}) 
  if(MPI_LINK_FLAGS)
    set_target_properties(autodiff_test002 PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
  endif() 
endif(USE_MPI)



add_test(NAME autodiff_test002_hessian_0 COMMAND $<TARGET_FILE:autodiff_test002> 0)
add_test(NAME autodiff_test002_hessian_1 COMMAND $<TARGET_FILE:autodiff_test002> 1)
add_test(NAME autodiff_test002_hessian_2 COMMAND $<TARGET_FILE:autodiff_test002> 2)
add_test(NAME autodiff_test002_hessian_3 COMMAND $<TARGET_FILE:autodiff_test002> 3)
add_test(NAME autodiff_test002_hessian_4 COMMAND $<TARGET_FILE:autodiff_test002> 4)
add_test(NAME autodiff_test002_hessian_5 COMMAND $<TARGET_FILE:autodiff_test002> 5)
add_test(NAME autodiff_test002_hessian_6 COMMAND $<TARGET_FILE:autodiff_test002> 6)
add_test(NAME autodiff_test002_hessian_7 COMMAND $<TARGET_FILE:autodiff_test002> 7)
