module OrionWorker

include("src/julia/dist_array_accessor.jl")
include("src/julia/dist_array.jl")
include("src/julia/dist_array_buffer.jl")
include("src/julia/dist_array_partition.jl")
include("src/julia/constants.jl")

function worker_init(_num_executors::Integer,
                     _num_servers::Integer)
    global const num_executors = _num_executors
    global const num_servers = _num_servers
    load_constants()
end

function helloworld()
    ccall((:orion_helloworld, lib_path), Void, ())
end

global_read_only_var_buff = Vector{Any}()

function clear_global_read_only_var_buff()
    resize!(global_read_only_var_buff, 0)
end

function resize_global_read_only_var_buff(size::UInt64)
    resize!(global_read_only_var_buff, size)
end

function global_read_only_var_buff_deserialize_and_set(index::UInt64, serialized_val::Vector{UInt8})::Any
    buff = IOBuffer(serialized_val)
    val = deserialize(buff)
    global_read_only_var_buff[index] = val
    return val
end

end

using OrionWorker

function orionres_define_dist_array(id::Int32,
                                    ValueType::Any,
                                    symbol::AbstractString,
                                    dims::Vector{Int64},
                                    is_dense::Bool,
                                    is_buffer::Bool,
                                    init_value)
    dist_array = OrionWorker.create_dist_array_on_worker(id,
                                                         ValueType,
                                                         symbol,
                                                         dims,
                                                         is_dense,
                                                         is_buffer,
                                                         init_value)
    dist_array_symbol = Symbol(symbol)
    eval(:(global $dist_array_symbol = $dist_array))
    return eval(dist_array_symbol)
end


function orionres_set_dist_array_dims(dist_array::OrionWorker.AbstractDistArray,
                                      dims::Vector{Int64})
    dist_array.dims = copy(dims)
end

function orionres_get_dist_array_value_type{T, N}(dist_array::OrionWorker.AbstractDistArray{T, N})::Any
    return T
end

function orionres_get_index(keys::Vector{Int64}, key::Int64, start::Int64)::Int64
    index = start
    while index <= length(keys) &&
        keys[index] != key
        index += 1
    end
    if index <= length(keys)
        return index
    else
        return -1
    end
end

function orionres_print_value_type(value)
    println(typeof(value))
end

function orionres_value_to_string(value)::String
    return string(value)
end
