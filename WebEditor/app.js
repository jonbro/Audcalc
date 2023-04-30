
'use strict';

let port;
let writer;
let inputDone;
let outputDone;
let inputStream;
let aboutToSend = false;
let readyForSend = false;
let waitingForDownloadComplete = false;
const chunks = [];

const log = document.getElementById('log');
const ledCBs = document.querySelectorAll('input.led');
const divLeftBut = document.getElementById('leftBut');
const divRightBut = document.getElementById('rightBut');
const butConnect = document.getElementById('butConnect');



document.addEventListener('DOMContentLoaded', () => {
  buttonConnect.addEventListener('click', clickConnect);
  buttonDownload.addEventListener('click', clickDownload);
  buttonUpload.addEventListener('change', clickUpload);
    // enable feature detection
  // const notSupported = document.getElementById('notSupported');
// notSupported.classList.toggle('hidden', 'serial' in navigator);
});

async function connect()
{
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    readLoop();
}

async function disconnect()
{
  drawGrid(GRID_OFF);
  sendGrid();
    // todo: close the port here
}


async function clickConnect()
{
  await connect();
}
let downloadArray;
function clickDownload()
{
    chunks.length = 0;
    hexDump = "";
    hexDumpCount = 0;
    waitingForDownloadComplete = true;
    downloadArray = new Uint8Array();
    writeToStream("/snds\0");
}
function readFile(event)
{
    const writer = port.writable.getWriter();
    writer.write(event.target.result);
    writer.releaseLock();
}

function clickUpload()
{
    // prepare for send
    aboutToSend = true;
    writeToStream("/rcvs\0");
}
let hexDump = "";
let hexDumpCount = 0;
let downloadURL = function(data, fileName)
{
    var a;
    a = document.createElement('a');
    a.href = data;
    a.download = fileName;
    document.body.appendChild(a);
    a.style = 'display: none';
    a.click();
    a.remove();
  };

async function readLoop()
{
    while (port.readable)
    {
        const reader = port.readable.getReader();
        try
        {
            while (true)
            {
                const { value, done } = await reader.read();
                if(waitingForDownloadComplete)
                {
                    // can't do push so we just reallocate
                    let original = downloadArray;
                    downloadArray = new Uint8Array(original.length + value.length);
                    downloadArray.set(original, 0);
                    downloadArray.set(value, original.length);
                    if(new TextDecoder().decode(downloadArray.slice(-7)).endsWith("/ready\0"))
                    {
                        waitingForDownloadComplete = false;
                        // debug the output
                        // for(let i=0;i<downloadArray.length;i++)
                        // {
                        //     hexDump += downloadArray[i].toString(16).padStart(2, '0') + " ";
                        //     hexDumpCount += 1;
                        //     if(hexDumpCount==16)
                        //     {
                        //         console.log(hexDump);
                        //         hexDump = "";
                        //         hexDumpCount = 0;
                        //     }    
                        // }
                        // if(hexDumpCount > 0)
                        // {
                        //     console.log(hexDump);
                        //     hexDump = "";
                        //     hexDumpCount = 0;
                        // }
                        let blob = new Blob([downloadArray]);
                        let url;
                        url = window.URL.createObjectURL(blob);
                        downloadURL(url, "song.bsn");
                        setTimeout(function() {
                        return window.URL.revokeObjectURL(url);
                        }, 1000);
                    }
                }
                else if(aboutToSend)
                {
                    if(new TextDecoder().decode(value).endsWith("/ready\0"))
                    {
                        var fileReader = new FileReader();
                        fileReader.addEventListener('load', readFile);
                        fileReader.readAsArrayBuffer(buttonUpload.files[0]);
                        aboutToSend = false;
                    }
                }
                if (done) {
                console.log('[readLoop] DONE', done);
                reader.releaseLock();
                break;
                }
            }
        } catch (error) {
            console.log(error);
            // Handle |error|...
        } finally {
            reader.releaseLock();
        }
    }
}


function writeToStream(...lines)
{
    const writer = port.writable.getWriter();
    lines.forEach((line) => {
        console.log('[SEND]', line);
        writer.write(new TextEncoder().encode(line + '\n'));
    });
    writer.releaseLock();
}