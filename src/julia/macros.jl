macro share(ex::Expr)
    eval_expr_on_all(ex, :Main)
    esc(ex)
end

macro parallel_for(args...)
    return parallelize_for_loop(args...)
end

macro accumulator(expr::Expr)
    @assert is_variable_definition(expr)
    var = assignment_get_assigned_to(expr)
    @assert isa(var, Symbol)
    accumulator_info_dict[var] = AccumulatorInfo(var,
                                                 deepcopy(eval(current_module(), assignment_get_assigned_from(expr))))
    ret = quote end
    push!(ret.args, esc(expr))
    var_str = string(var)
    define_var_expr = :(Orion.define_var(Symbol($var_str)))
    push!(ret.args, define_var_expr)
    return ret
end

function parallelize_for_loop(args...)
    @assert length(args) > 0

    loop_stmt = args[end]
    is_ordered = false
    is_repeated = false
    is_histogram_partitioned = false
    is_prefetch_disabled = false
    to_reassign_iteration_var_val = false

    for arg in args[1:(length(args) - 1)]
        @assert isa(arg, Symbol)
        if arg == :ordered
            is_ordered = true
        elseif arg == :repeated
            is_repeated = true
        elseif arg == :histogram_partitioned
            is_histogram_partitioned = true
        elseif arg == :prefetch_distabled
            is_prefetch_disabled = true
        elseif arg == :reassign_iteration_var_val
            to_reassign_iteration_var_val = true
        else
            error("unrecognized specifier ", arg)
        end
    end

    println("parallelize_for loop")
    @assert is_for_loop(loop_stmt)
    iteration_var = for_get_iteration_var(loop_stmt)
    iteration_space = for_get_iteration_space(loop_stmt)
    @assert isa(iteration_var, Expr) && iteration_var.head == :tuple
    iteration_var_key = iteration_var.args[1]
    iteration_var_val = iteration_var.args[2]

    @assert isa(iteration_space, Symbol)
    @assert isdefined(current_module(), iteration_space)
    @assert isa(eval(current_module(), iteration_space), DistArray)

    # find variables that need to be broadcast and marked global
    @time scope_context = get_scope_context!(nothing, loop_stmt)
    global_read_only_vars = get_global_read_only_vars(scope_context)
    accumulator_vars = get_accumulator_vars(scope_context)

    loop_body = for_get_loop_body(loop_stmt)
    @time (flow_graph, _, ssa_context) = flow_analysis(loop_body)

    parallelized_loop = quote end
    println("before static_parallelize")
    exec_loop_stmts = static_parallelize(iteration_space,
                                         iteration_var_key,
                                         iteration_var_val,
                                         global_read_only_vars,
                                         accumulator_vars,
                                         loop_body,
                                         is_ordered,
                                         is_repeated,
                                         is_histogram_partitioned,
                                         is_prefetch_disabled,
                                         to_reassign_iteration_var_val,
                                         ssa_context.ssa_defs,
                                         flow_graph)

    if exec_loop_stmts == nothing
        error("loop not parallelizable")
    end
    push!(parallelized_loop.args, exec_loop_stmts)
    return parallelized_loop
end

macro dist_array(expr::Expr)
    ret_stmts = quote
        $(esc(expr))
    end

    @assert expr.head == :(=)
    dist_array_symbol = assignment_get_assigned_to(expr)
    @assert isa(dist_array_symbol, Symbol)
    symbol_str = string(dist_array_symbol)
    push!(ret_stmts.args,
          :(Orion.dist_array_set_symbol($(esc(dist_array_symbol)), $symbol_str)))
    return ret_stmts
end
