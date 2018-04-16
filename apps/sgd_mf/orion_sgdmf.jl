include("/users/jinlianw/orion.git/src/julia/orion.jl")

println("application started")

# set path to the C++ runtime library
Orion.set_lib_path("/users/jinlianw/orion.git/lib/liborion_driver.so")
# test library path
Orion.helloworld()

#const master_ip = "10.117.1.3"
const master_ip = "127.0.0.1"
const master_port = 10000
const comm_buff_capacity = 1024
const num_executors = 1
const num_servers = 1

# initialize logging of the runtime library
Orion.glog_init()
Orion.init(master_ip, master_port, comm_buff_capacity,
           num_executors, num_servers)

#const data_path = "file:///home/ubuntu/data/ml-1m/ratings.csv"
#const data_path = "file:///home/ubuntu/data/ml-10M100K/ratings.csv"
const data_path = "file:///users/jinlianw/ratings.csv"
#const data_path = "file:///proj/BigLearning/jinlianw/data/netflix.csv"
#const data_path = "file:///proj/BigLearning/jinlianw/data/ml-20m/ratings_p.csv"
const K = 1000
const num_iterations = 2
const step_size = Float32(0.02)

Orion.@accumulator err = 0
Orion.@accumulator line_cnt = 0

Orion.@share function parse_line(line::AbstractString)
    global line_cnt
    line_cnt += 1
    tokens = split(line, ',')
    @assert length(tokens) == 3
    key_tuple = (parse(Int64, String(tokens[1])),
                 parse(Int64, String(tokens[2])) )
    value = parse(Float32, String(tokens[3]))
    return (key_tuple, value)
end

Orion.@share function map_init_param(value::Float32)::Float32
    return value / 10
end

Orion.@dist_array ratings = Orion.text_file(data_path, parse_line)
Orion.materialize(ratings)
dim_x, dim_y = size(ratings)

println((dim_x, dim_y))
line_cnt = Orion.get_aggregated_value(:line_cnt, :+)
println("line_cnt = ", line_cnt)

Orion.@dist_array W = Orion.randn(K, dim_x)
Orion.@dist_array W = Orion.map(W, map_init_param, map_values = true)
Orion.materialize(W)

Orion.@dist_array H = Orion.randn(K, dim_y)
Orion.@dist_array H = Orion.map(H, map_init_param, map_values = true)
Orion.materialize(H)

#Orion.dist_array_set_num_partitions_per_dim(ratings, num_executors * 4)

error_vec = Vector{Float64}()
time_vec = Vector{Float64}()
start_time = now()

W_grad = zeros(K)
H_grad = zeros(K)

@time for iteration = 1:num_iterations
    Orion.@parallel_for for rating in ratings
        x_idx = rating[1][1]
        y_idx = rating[1][2]
        rv = rating[2]

        W_row = @view W[:, x_idx]
        H_row = @view H[:, y_idx]
        pred = dot(W_row, H_row)
        diff = rv - pred
        W_grad .= -2 * diff .* H_row
        H_grad .= -2 * diff .* W_row
        W[:, x_idx] .= W_row .- step_size .* W_grad
        H[:, y_idx] .= H_row .- step_size .* H_grad
    end
    @time if iteration % 4 == 1 ||
        iteration == num_iterations
        println("evaluate model")
        Orion.@parallel_for for rating in ratings
            x_idx = rating[1][1]
            y_idx = rating[1][2]
            rv = rating[2]
            W_row = @view W[:, x_idx]
            H_row = @view H[:, y_idx]
            pred = dot(W_row, H_row)
            err += (rv - pred) ^ 2
        end
        err = Orion.get_aggregated_value(:err, :+)
        curr_time = now()
        elapsed = Int(Dates.value(curr_time - start_time)) / 1000
        println("iteration = ", iteration, " elapsed = ", elapsed, " err = ", err)
        Orion.reset_accumulator(:err)
        push!(error_vec, err)
        push!(time_vec, elapsed)
    end
end
println(error_vec)
println(time_vec)
Orion.stop()
exit()
