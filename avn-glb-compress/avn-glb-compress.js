#!/usr/bin/env node

const yargs = require("yargs");
const fs = require("fs");

const options = yargs
 .usage("Usage: -i <input GLB> -o <output GLB> -c <full|preview>")
 .option("i", { alias: "input", describe: "GLB file to compress", type: "string", demandOption: true })
 .option("o", { alias: "output", describe: "Output GLB file", type: "string", demandOption: true })
 .option("c", { alias: "compression", describe: "Compression level - full/preview (default)", type: "string"})
 .argv;

const exec = require("child_process");
const tmpGLTF = "model.gltf"
const gm = require("gm");

let rescaledImgArray = new Array();

const processAllImages = async function(imgCount)  {
	var files = fs.readdirSync('.');

  let n = 1;
	//files.forEach(async (file) => {
	for (const file of files) {
		if (file.endsWith(".jpg") || file.endsWith(".jpeg") || file.endsWith(".png")) {
      console.log(`Processing Image (${n} of ${imgCount}) - "${file}"`);
			await processTexture(file);
      n++;
		}
	}

	console.log(`Repacking GLB to -> ${options.output}`);
	exec.execSync(`gltf-pipeline -i ${tmpGLTF} -o ../${options.output}`, {stdio: 'inherit'});

  if (0 < rescaledImgArray.length) {
    console.log("\n*** WARNING ***");
    console.log(`Found ${rescaledImgArray.length} texture images requiring re-scale. The files below need re-dimensioning...`);
    for (let n = 0 ; n < rescaledImgArray.length ; n++) {
      let fileInfo = rescaledImgArray[n];
      console.log("\t" + fileInfo);
    }
  }
}

if (options.compression === "full") {
  console.log(`Processing GLB with FULL compression- ${options.input} -> ${options.output}`);
} else {
  console.log(`Processing GLB with PREVIEW compression- ${options.input} -> ${options.output}`);
}

let tmpFldr = "temp-" + options.input;
if (!fs.existsSync(tmpFldr)) {
	fs.mkdirSync(tmpFldr);
}
process.chdir(tmpFldr);

exec.execSync(`gltf-pipeline -i ../${options.input} -o ${tmpGLTF} -t`, {stdio: 'inherit'});

let rawdata = fs.readFileSync(tmpGLTF);
let strJson = rawdata.toString();

let pngCount = occurrences(strJson, '.png"', false);
let jpgCount = occurrences(strJson, '.jpg"', false);
let jpegCount = occurrences(strJson, '.jpeg"', false);

console.log(`GLTF refers to ${pngCount} .png(s), ${jpgCount} .jpg(s), ${jpegCount} .jpeg(s)`);

let gltfDoc = JSON.parse(rawdata);

let imgs = gltfDoc["images"];
let imgCount = imgs.length;
console.log(`GLTF contains ${imgCount} images`);

imgs.forEach(img => {
	if (img.uri.endsWith(".jpg") || img.uri.endsWith(".jpeg") || img.uri.endsWith(".png")) {
		let uri = img.uri;
		if (uri.endsWith(".jpg")) {
			uri = uri.replace(/\.jpg$/gi, ".basis");
		} else if (uri.endsWith(".jpeg")) {
			uri = uri.replace(/\.jpeg$/gi, ".basis");
		} else if (uri.endsWith(".png")) {
			uri = uri.replace(/\.png$/gi, ".basis");
		}

		console.log(`Image (${img.mimeType}) URI: "${img.uri}" -> "${uri}"`);
		img.uri = uri;
	} else {
		console.log(`UNEXPECTED Image URI - ${img.uri} - unrecognised file extension`);
	}
});

let txtrs = gltfDoc["textures"];
console.log(`GLTF contains ${txtrs.length} textures`);

txtrs.forEach(txtr => {
	let src = txtr["source"];
	//console.log(`Texture source - ${src}`);
	var exts = new Object();
	var extBasis = new Object();
	extBasis.source = src;
	exts.MOZ_HUBS_texture_basis = extBasis;

	txtr.extensions = exts;
});

var extsUsed = new Array();
if (gltfDoc.hasOwnProperty("extensionsUsed")) {
	extsUsed = gltfDoc.extensionsUsed;
}
//console.log(`Extensions Used array initially contains ${extsUsed.length} entries`);
extsUsed[extsUsed.length] = "MOZ_HUBS_texture_basis";
//console.log(`After Basis extension added, array contains ${extsUsed.length} entries`);

gltfDoc.extensionsUsed = extsUsed;

let outputJson = JSON.stringify(gltfDoc, null, 2);
fs.writeFileSync(tmpGLTF, outputJson);

// Now convert all the images to the basis format.
console.log("");
processAllImages(imgCount);

function isPowerOf2(i) {
	return (i & (i - 1)) == 0;
}

function highestPowerOf2Below(n) {
	for (i = n ; i >= 1 ; i--) {
		if (isPowerOf2(i))
			return i;
	}
}

async function resizeImage (f, w, h) {
	return new Promise ((resolve, reject) => {
		fs.copyFileSync(f, "original-" + f);

		gm(f)
		.resize(w, h, '!')
		.write(f, function(err) {
			console.log(`Resizing Texture Image - "${f}"...`);

			if(err) {
				console.log("GM ERROR: " + err);
				reject(err);
			}
			resolve();
		});
	});
}

async function processTexture(imgFile) {
	return new Promise ((resolve, reject) => {
		gm(imgFile).size(async function(err, imgSz) {
			if ( (!isPowerOf2(imgSz.width)) || (!isPowerOf2(imgSz.height)) ) {
				let newWidth = highestPowerOf2Below(imgSz.width);
				let newHeight = highestPowerOf2Below(imgSz.height);

        let fileInfo = `"${imgFile}" - width: ${imgSz.width} => ${newWidth}, height: ${imgSz.height} => ${newHeight}`;

				console.log("MUST RESIZE Texture -- " + fileInfo);

        rescaledImgArray.push(fileInfo);

				await resizeImage(imgFile, newWidth, newHeight);
			}

      let basiscmd = `\.\./basisu -linear -mipmap -individual -file "${imgFile}"`
      if (options.compression === "full") {
		      basiscmd = `\.\./basisu -linear -mipmap -individual -max_endpoints 16128 -max_selectors 16128 -comp_level 5 -file "${imgFile}"`
      }
			exec.execSync(basiscmd, {stdio: 'inherit'});

			if(err) {
				console.log("GM ERROR: " + err);
				reject(err);
			}
			resolve(imgFile);
		});

	});
}


/** Function that count occurrences of a substring in a string;
 * @param {String} string               The string
 * @param {String} subString            The sub string to search for
 * @param {Boolean} [allowOverlapping]  Optional. (Default:false)
 *
 * @author Vitim.us https://gist.github.com/victornpb/7736865
 * @see Unit Test https://jsfiddle.net/Victornpb/5axuh96u/
 * @see http://stackoverflow.com/questions/4009756/how-to-count-string-occurrence-in-string/7924240#7924240
 */
function occurrences(string, subString, allowOverlapping) {

    string += "";
    subString += "";
    if (subString.length <= 0) return (string.length + 1);

    var n = 0,
        pos = 0,
        step = allowOverlapping ? 1 : subString.length;

    while (true) {
        pos = string.indexOf(subString, pos);
        if (pos >= 0) {
            ++n;
            pos += step;
        } else break;
    }
    return n;
}
