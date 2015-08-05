## Don't delete the buffer that contains the output of the command that downloaded the file
decl bool autodownload_keep_log no

## Format of the download string that will be used to fetch the file
## Defaults to autodownload_format_wget
decl str autodownload_format

## Pre-defined formats for different popular download tools
decl str autodownload_format_wget "wget -o '{progress}' -O '{output}' '{url}'"
decl str autodownload_format_aria2 "aria2c -o $(basename '{output}') -d $(dirname '{output}') '{url}' > '{progress}'"
decl str autodownload_format_curl "curl -o '{output}' '{url}' 2> '{progress}'"

## Set the default downloader to be wget
set global autodownload_format %opt{autodownload_format_wget}

hook global BufNew .* %{
    %sh{
        readonly netproto_url="${kak_hook_param}"
        readonly netproto_proto="${netproto_url%:*}"

        ## Check that the downloader used is reachable from this shell
        type "${kak_opt_autodownload_format%% *}" &>/dev/null || exit

        ## Check that a url was passed to kakoune
        [[ "${netproto_url}" =~ [a-zA-Z0-9]+://.+ ]] || exit

        ## Create a temporary directory in which we will download the file
        readonly path_dir_tmp=$(mktemp -d -t kak-proto.XXXXXXXX)
        test ! -z "${path_dir_tmp}" || {
            echo "echo -color Error Unable to create temporary directory";
            exit;
        }

        readonly netproto_buffer="${path_dir_tmp}/buffer"
        readonly netproto_fifo="${path_dir_tmp}/fifo"

		## Create a named pipe that will print the download status
        mkfifo "${netproto_fifo}" || {
            echo "echo -color Error Unable to create named pipe";
            exit;
        }

        readonly buffer_basename="${netproto_url##*/}"

		## Start downloading the file to a temporary directory
		## When the download has finished, remove the pipe to notify the hook below that the file can be loaded
        (
            download_str=$(sed "s/{url}/${netproto_url//\//\\\/}/g; \
                                s/{progress}/${netproto_fifo//\//\\\/}/g; \
                                s/{output}/${netproto_buffer//\//\\\/}/g" <<< "${kak_opt_autodownload_format}")
            eval "${download_str}"
            rm -f "${netproto_fifo}"
        ) &>/dev/null </dev/null &

		## Open a new buffer who will read and print the download's progress
		## Remove the original buffer that was named after the URL of the file to fetch
		## When the file has been downloaded, create its own buffer and remove temporary files
		## If the user doesn't want to have the download progress kept in its own buffer after
		## the download has finished, we remove that buffer
        echo "
            eval %{
                edit! -fifo '${netproto_fifo}' -scroll 'download:${netproto_url}'
                delbuf! '${netproto_url}'
                hook -group fifo buffer BufCloseFifo .* %{
                    edit '${buffer_basename}'
                    exec '%d!cat<space>${netproto_buffer}<ret>d'
                    %sh{
                        rm -rf '${path_dir_tmp}'
                        if [ '${kak_opt_autodownload_keep_log,,}' != true ]; then
                            echo '
                                delbuf! download:${netproto_url}
                                buffer ${buffer_basename}
                            '
                        fi
                    }
                    rmhooks buffer fifo
                }
            }
        "
    }
}
