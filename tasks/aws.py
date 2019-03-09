from datetime import datetime
from json import dumps
from os import mkdir
from os.path import join, exists
from shutil import rmtree
from subprocess import call
from time import sleep

import boto3
from botocore.exceptions import ClientError
from invoke import task

from tasks.config import get_faasm_config
from tasks.env import FAASM_HOME, PROJ_ROOT, RUNTIME_S3_BUCKET, AWS_REGION, AWS_ACCOUNT_ID, STATE_S3_BUCKET, \
    WASM_FUNC_BUILD_DIR
from tasks.upload_util import upload_file_to_s3

SDK_VERSION = "1.7.41"
RUNTIME_VERSION = "master"

INSTALL_PATH = join(FAASM_HOME, "lambda")

FAASM_LAMBDA_BUILD_DIR = join(PROJ_ROOT, "lambda_faasm_build")

AWS_LAMBDA_ROLE = "faasm-lambda-role"

faasm_lambda_funcs = {
    "worker": {
        "name": "faasm-worker",
        "memory": 2688,
        # Worker timeout should be less than the function timeout to give things time to shut down gracefully
        "timeout": 600,
        "concurrency": 2,
        "extra_env": {
            "GLOBAL_MESSAGE_TIMEOUT": "120000",
            "UNBOUND_TIMEOUT": "30000",
            "THREADS_PER_WORKER": "10",
        },
        "sqs": True,
    },
    "dispatch": {
        "name": "faasm-dispatch",
        "memory": 128,
        "timeout": 30,
        "concurrency": 2,
        "extra_env": {
        }
    },
    "codegen": {
        "name": "faasm-codegen",
        "memory": 300,
        "timeout": 60,
        "concurrency": 1,
        "extra_env": {
        }
    },
    "state": {
        "name": "faasm-state",
        "memory": 2048,
        "timeout": 300,
        "concurrency": 1,
        "extra_env": {
            "BUCKET_NAME": STATE_S3_BUCKET,
        }
    },
    "redis": {
        "name": "faasm-redis",
        "memory": 128,
        "timeout": 15,
        "concurrency": 1,
        "extra_env": {

        }
    }
}


def _get_lambda_env():
    conf = get_faasm_config()
    state_redis_url = conf["AWS"]["redis_state"]
    queue_redis_url = conf["AWS"]["redis_queue"]

    env = {
        "FUNCTION_STORAGE": "s3",
        "HOST_TYPE": "lambda",
        "LOG_LEVEL": "debug",
        "CGROUP_MODE": "off",
        "NETNS_MODE": "off",
        "BUCKET_NAME": "faasm-runtime",
        "QUEUE_NAME": "faasm-messages",
        "SERIALISATION": "proto",
        "REDIS_STATE_HOST": state_redis_url,
        "REDIS_QUEUE_HOST": queue_redis_url,
        "NO_SCHEDULER": "1",
        "GLOBAL_MESSAGE_BUS": "redis",
        "AWS_LOG_LEVEL": "info",
        "FULL_ASYNC": "1",
    }

    return env


# --------------------------------
# SQS
# --------------------------------

def _get_sqs_client():
    conf = get_faasm_config()
    url = conf["AWS"]["sqs_url"]
    client = boto3.client("sqs", region_name=AWS_REGION)
    return client, url


@task
def sqs_length(ctx):
    client, url = _get_sqs_client()

    response = client.get_queue_attributes(
        QueueUrl=url,
        AttributeNames=["ApproximateNumberOfMessages", "ApproximateNumberOfMessagesNotVisible"]
    )

    queued = response["Attributes"]["ApproximateNumberOfMessages"]
    in_flight = response["Attributes"]["ApproximateNumberOfMessagesNotVisible"]

    total = int(queued) + int(in_flight)

    print("Queue length: {}".format(total))


@task
def purge_sqs(ctx):
    client, url = _get_sqs_client()
    client.purge_queue(
        QueueUrl=url,
    )


@task
def invoke_lambda_worker(ctx):
    client, url = _get_sqs_client()
    print("Adding message to queue {}".format(url))

    message = {
        "submitted": str(datetime.now()),
    }

    client.send_message(
        QueueUrl=url,
        MessageBody=dumps(message),
    )


# --------------------------------
# INVOKE/ LIST
# --------------------------------

@task
def list_lambdas(ctx):
    client = boto3.client("lambda", region_name=AWS_REGION)

    result = client.list_functions()

    for func_data in result["Functions"]:
        print("\n--- {} --- ".format(func_data["FunctionName"]))
        print("Timeout:  {}".format(func_data["Timeout"]))
        print("Memory:   {}MB".format(func_data["MemorySize"]))
        print("Modified: {}".format(func_data["LastModified"]))


@task
def list_event_sources(ctx, func_name):
    client = boto3.client("lambda", region_name=AWS_REGION)

    conf = get_faasm_config()
    sqs_arn = conf["AWS"]["sqs_arn"]

    response = client.list_event_source_mappings(
        EventSourceArn=sqs_arn,
        FunctionName=func_name,
    )

    for es in response["EventSourceMappings"]:
        print(es)


@task
def lambda_worker_count(ctx):
    response_body = invoke_lambda(ctx, "faasm-redis", payload={
        "target": "worker-count",
    })

    response_body = response_body.replace("Worker count: ", "")
    return int(response_body.strip())


@task
def lambda_clear_queue(ctx):
    invoke_lambda(ctx, "faasm-redis", payload={
        "target": "flush-queue",
    })


@task
def invoke_lambda(ctx, lambda_name, async=False, payload=None):
    client = boto3.client("lambda", region_name=AWS_REGION)

    kwargs = {
        "FunctionName": lambda_name,
        "InvocationType": "Event" if async else "RequestResponse",
    }

    payload = payload if payload else {}
    payload["submitted"] = str(datetime.now())
    kwargs["Payload"] = dumps(payload)

    response = client.invoke(**kwargs)

    print(response)
    response_body = response["Payload"].read().decode()
    print("\nResponse payload: {}".format(response_body))

    return response_body


@task
def invoke_faasm_lambda(ctx, user, func, input_data="", extra_payload=None, async=False):
    payload = {
        "user": user,
        "function": func,
        "input_data": input_data,
    }

    if extra_payload:
        payload.update(extra_payload)

    invoke_lambda(ctx, "faasm-dispatch", async=async, payload=payload)


# --------------------------------
# SYSTEM LAMBDAS
# --------------------------------

@task
def prepare_lambda_workers(ctx, n_workers):
    n_workers = int(n_workers)

    # First take concurrency down to zero
    lambda_concurrency(ctx, "worker", 0)
    sleep(5)

    # Clear out the queue
    lambda_clear_queue(ctx)

    # Now up concurrency
    lambda_concurrency(ctx, "worker", n_workers)

    # Now trigger messages
    for i in range(n_workers):
        invoke_lambda_worker(ctx)

    # Wait and check the worker count
    sleep(10)
    worker_count = lambda_worker_count(ctx)

    if worker_count == n_workers:
        print("Successfully set-up ({} workers)".format(n_workers))
    else:
        print("Unsuccessful set-up (expected {} but found {})".format(n_workers, worker_count))


@task
def lambda_concurrency(ctx, func_name, concurrency):
    conf = faasm_lambda_funcs[func_name]

    concurrency = int(concurrency)
    client = boto3.client("lambda", region_name=AWS_REGION)

    print("Setting concurrency for {} to {}".format(conf["name"], concurrency))

    client.put_function_concurrency(
        FunctionName=conf["name"],
        ReservedConcurrentExecutions=concurrency,
    )


@task
def deploy_faasm_lambda(ctx, func=None):
    if func:
        _do_system_lambda_deploy(func, faasm_lambda_funcs[func])
    else:
        for func_name, lambda_conf in faasm_lambda_funcs.items():
            _do_system_lambda_deploy(func_name, lambda_conf)


@task
def delete_faasm_lambda(ctx, func=None):
    if func:
        _delete_system_lambda(func)
    else:
        for lambda_func, _ in faasm_lambda_funcs.items():
            _delete_system_lambda(lambda_func)


def _do_system_lambda_deploy(func_name, lambda_conf):
    print("\n---------------------")
    print("Build and deploy {} lambda func".format(func_name))
    print("---------------------\n")

    _build_system_lambda(func_name)

    s3_key = _get_s3_key(func_name)

    lambda_env = _get_lambda_env()
    lambda_env.update(lambda_conf["extra_env"])

    _do_deploy(
        lambda_conf["name"],
        s3_bucket=RUNTIME_S3_BUCKET,
        s3_key=s3_key,
        memory=lambda_conf["memory"],
        timeout=lambda_conf["timeout"],
        concurrency=lambda_conf["concurrency"],
        environment=lambda_env,
        sqs=lambda_conf.get("sqs", False),
    )


def _delete_system_lambda(func_name):
    lambda_name = faasm_lambda_funcs[func_name]["name"]
    client = boto3.client("lambda", region_name=AWS_REGION)

    print("Deleting lambda function {}".format(lambda_name))
    client.delete_function(
        FunctionName=lambda_name,
    )


def _build_system_lambda(module_name):
    build_dir = _build_lambda(module_name)
    _create_lambda_zip(module_name, build_dir)

    zip_file_path = join(
        build_dir,
        "src",
        module_name,
        "{}-lambda.zip".format(module_name)
    )

    assert exists(zip_file_path), "Could not find lambda zip at {}".format(zip_file_path)

    s3_key = _get_s3_key(module_name)
    _upload_lambda_to_s3(s3_key, zip_file_path)


# -------------------------------------------------
# WASM LAMBDA FUNCTIONS
# -------------------------------------------------

@task
def deploy_wasm_lambda_func(ctx, user, func):
    deploy_wasm_lambda_func_multiple(ctx, user, [func])


def deploy_wasm_lambda_func_multiple(ctx, user, funcs):
    s3_client = boto3.resource('s3', region_name=AWS_REGION)

    for func in funcs:
        # Assume the build has already been run locally
        wasm_file = join(WASM_FUNC_BUILD_DIR, user, "{}.wasm".format(func))

        print("Uploading {}/{} to S3".format(user, func))

        s3_key = "wasm/{}/{}/function.wasm".format(user, func)
        s3_client.meta.client.upload_file(wasm_file, RUNTIME_S3_BUCKET, s3_key)

        print("Invoking codegen lambda function for {}/{}".format(user, func))
        invoke_lambda(ctx, "faasm-codegen", payload={
            "user": user,
            "function": func,
        })


# -------------------------------------------------
# NATIVE LAMBDA FUNCTIONS
# -------------------------------------------------

@task
def deploy_native_lambda_func(ctx, user, func):
    # Run the build
    build_dir = _build_lambda("func", target="{}-lambda".format(func))

    # Create the function zip
    _create_lambda_zip(func, build_dir)

    zip_file_path = join(
        build_dir,
        "func",
        user,
        "{}-lambda.zip".format(func)
    )

    assert exists(zip_file_path), "Could not find lambda zip at {}".format(zip_file_path)

    s3_key = _get_s3_key("{}-{}".format(user, func))
    _upload_lambda_to_s3(s3_key, zip_file_path)

    _do_deploy(
        "{}-{}".format(user, func),
        s3_bucket=RUNTIME_S3_BUCKET,
        s3_key=s3_key,
        environment=_get_lambda_env()
    )


# -------------------------------------------
# MISC UTILITIES
# -------------------------------------------

def _build_lambda(module_name, target=None):
    print("Running Lambda {} build".format(module_name))

    # Compile the whole project passing in the right config
    build_dir = FAASM_LAMBDA_BUILD_DIR
    _build_cmake_project(build_dir, [
        "-DFAASM_BUILD_TYPE=lambda-{}".format(module_name),
        "-DCMAKE_BUILD_TYPE=Release",
    ], clean=False, target=target)

    return build_dir


def _do_deploy(func_name, memory=128, timeout=15, concurrency=10,
               environment=None, zip_file_path=None, s3_bucket=None, s3_key=None, sqs=False):
    assert zip_file_path or (s3_bucket and s3_key), "Must give either a zip file or S3 bucket and key"

    client = boto3.client("lambda", region_name=AWS_REGION)

    if zip_file_path:
        assert exists(zip_file_path), "Expected zip file at {}".format(zip_file_path)

    # Get subnet IDs and security groups
    conf = get_faasm_config()
    subnet_ids = conf["AWS"]["subnet_ids"].split(",")
    security_group_ids = conf["AWS"]["security_group_ids"].split(",")

    # Check if function exists
    is_existing = True

    try:
        client.get_function(
            FunctionName=func_name,
        )
    except ClientError:
        is_existing = False

    kwargs = {
        "FunctionName": func_name,
    }

    content = None
    if zip_file_path:
        with open(zip_file_path, "rb") as fh:
            content = fh.read()

    if is_existing:
        print("{} already exists, updating".format(func_name))

        if zip_file_path:
            kwargs["ZipFile"] = content
        else:
            kwargs["S3Bucket"] = s3_bucket
            kwargs["S3Key"] = s3_key

        client.update_function_code(**kwargs)
    else:
        print("{} does not already exist, creating".format(func_name))

        kwargs.update({
            "Runtime": "provided",
            "Role": "arn:aws:iam::{}:role/{}".format(AWS_ACCOUNT_ID, AWS_LAMBDA_ROLE),
            "Handler": func_name,
            "MemorySize": memory,
            "Timeout": timeout,
            "VpcConfig": {
                "SubnetIds": subnet_ids,
                "SecurityGroupIds": security_group_ids
            }
        })

        if zip_file_path:
            kwargs["Code"] = {"ZipFile": content}
        else:
            kwargs["Code"] = {"S3Bucket": s3_bucket, "S3Key": s3_key}

        if environment:
            lambda_env = {
                "Variables": environment
            }
            kwargs["Environment"] = lambda_env

        client.create_function(**kwargs)

    # Set up concurrency
    client.put_function_concurrency(
        FunctionName=func_name,
        ReservedConcurrentExecutions=concurrency,
    )

    if sqs:
        _add_sqs_event_source(client, func_name)


def _create_lambda_zip(module_name, build_dir):
    cmake_zip_target = "aws-lambda-package-{}-lambda".format(module_name)
    res = call("make {}".format(cmake_zip_target), cwd=build_dir, shell=True)
    if res != 0:
        raise RuntimeError("Failed to create lambda zip")


def _get_s3_key(module_name):
    s3_key = "{}-lambda.zip".format(module_name)
    return s3_key


def _upload_lambda_to_s3(s3_key, zip_file_path):
    print("Uploading lambda {} to S3".format(s3_key))
    upload_file_to_s3(zip_file_path, RUNTIME_S3_BUCKET, s3_key)


def _build_cmake_project(build_dir, cmake_args, clean=False, target=None):
    # Nuke if required
    if clean and exists(build_dir):
        rmtree(build_dir)

    # Create build dir if necessary
    if not exists(build_dir):
        mkdir(build_dir)

    cpp_sdk_build_cmd = [
        "cmake",
        "..",
        "-DCMAKE_INSTALL_PREFIX={}".format(INSTALL_PATH),
    ]

    cpp_sdk_build_cmd.extend(cmake_args)

    res = call(" ".join(cpp_sdk_build_cmd), cwd=build_dir, shell=True)
    if res != 0:
        raise RuntimeError("Building cmake project failed")

    if target:
        call("make {}".format(target), cwd=build_dir, shell=True)
    else:
        call("make", cwd=build_dir, shell=True)


def _add_sqs_event_source(client, func_name):
    conf = get_faasm_config()
    queue_arn = conf["AWS"]["sqs_arn"]
    queue_url = conf["AWS"]["sqs_url"]

    print("Adding SQS source for {} from queue {}".format(func_name, queue_url))

    # List existing
    response = client.list_event_source_mappings(
        EventSourceArn=queue_arn,
        FunctionName=func_name,
        MaxItems=1
    )

    if len(response["EventSourceMappings"]) > 0:
        uuid = response["EventSourceMappings"][0]["UUID"]
        print("Already have event source mapping, attempting to update UUID ", uuid)

        response = client.update_event_source_mapping(
            UUID=uuid,
            FunctionName=func_name,
            Enabled=True,
            BatchSize=1
        )

    else:
        print("Creating new event source mapping")
        client.create_event_source_mapping(
            EventSourceArn=queue_arn,
            FunctionName=func_name,
            Enabled=True,
            BatchSize=1,
        )
